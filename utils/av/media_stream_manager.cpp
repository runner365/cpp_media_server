#include "media_stream_manager.hpp"
#include "media_packet.hpp"
#include "logger.hpp"
#include <sstream>
#include <vector>

std::unordered_map<std::string, MEDIA_STREAM_PTR> media_stream_manager::media_streams_map_;
std::vector<stream_manager_callbackI*> media_stream_manager::cb_vec_;
av_writer_base* media_stream_manager::hls_writer_ = nullptr;
av_writer_base* media_stream_manager::r2r_writer_ = nullptr;

bool media_stream_manager::get_app_streamname(const std::string& stream_key, std::string& app, std::string& streamname) {
    size_t pos = stream_key.find("/");
    if (pos == stream_key.npos) {
        return false;
    }
    app = stream_key.substr(0, pos);
    streamname = stream_key.substr(pos+1);

    return true;
}

int media_stream_manager::add_player(av_writer_base* writer_p) {
    std::string key_str = writer_p->get_key();
    std::string writerid = writer_p->get_writerid();

    std::unordered_map<std::string, MEDIA_STREAM_PTR>::iterator iter = media_streams_map_.find(key_str);
    if (iter == media_stream_manager::media_streams_map_.end()) {
        MEDIA_STREAM_PTR new_stream_ptr = std::make_shared<media_stream>();

        new_stream_ptr->writer_map_.insert(std::make_pair(writerid, writer_p));
        media_stream_manager::media_streams_map_.insert(std::make_pair(key_str, new_stream_ptr));
        log_infof("add player request:%s in new writer list", key_str.c_str());
        return 1;
    }

    log_infof("add player request:%s, stream_p:%p",
        key_str.c_str(), (void*)iter->second.get());
    iter->second->writer_map_.insert(std::make_pair(writerid, writer_p));
    return iter->second->writer_map_.size();
}

void media_stream_manager::remove_player(av_writer_base* writer_p) {
    std::string key_str = writer_p->get_key();
    std::string writerid = writer_p->get_writerid();

    log_infof("remove player key:%s", key_str.c_str());
    auto map_iter = media_streams_map_.find(key_str);
    if (map_iter == media_streams_map_.end()) {
        log_warnf("it's empty when remove player:%s", key_str.c_str());
        return;
    }

    auto writer_iter = map_iter->second->writer_map_.find(writerid);
    if (writer_iter != map_iter->second->writer_map_.end()) {
        map_iter->second->writer_map_.erase(writer_iter);
    }
    
    if (map_iter->second->writer_map_.empty() && !map_iter->second->publisher_exist_) {
        //playlist is empty and the publisher does not exist
        media_streams_map_.erase(map_iter);
        log_infof("delete stream %s for the publisher and players are empty.", key_str.c_str());
    }
    return;
}

MEDIA_STREAM_PTR media_stream_manager::add_publisher(const std::string& stream_key) {
    MEDIA_STREAM_PTR ret_stream_ptr;

    auto iter = media_streams_map_.find(stream_key);
    if (iter == media_streams_map_.end()) {
        ret_stream_ptr = std::make_shared<media_stream>();
        ret_stream_ptr->publisher_exist_ = true;
        ret_stream_ptr->stream_key_ = stream_key;
        log_infof("add new publisher stream key:%s, stream_p:%p",
            stream_key.c_str(), (void*)ret_stream_ptr.get());
        media_streams_map_.insert(std::make_pair(stream_key, ret_stream_ptr));

        std::string app;
        std::string streamname;
        if (get_app_streamname(stream_key, app, streamname)) {
            for(auto cb : cb_vec_) {
                cb->on_publish(app, streamname);
            }
        }

        return ret_stream_ptr;
    }
    ret_stream_ptr = iter->second;
    ret_stream_ptr->publisher_exist_ = true;
    return ret_stream_ptr;
}

void media_stream_manager::remove_publisher(const std::string& stream_key) {
    auto iter = media_streams_map_.find(stream_key);
    if (iter == media_streams_map_.end()) {
        log_warnf("There is not publish key:%s", stream_key.c_str());
        return;
    }

    log_infof("remove publisher in media stream:%s", stream_key.c_str());
    iter->second->publisher_exist_ = false;
    if (iter->second->writer_map_.empty()) {
        log_infof("delete stream %s for the publisher and players are empty.", stream_key.c_str());
        media_streams_map_.erase(iter);
    }

    std::string app;
    std::string streamname;
    if (get_app_streamname(stream_key, app, streamname)) {
        for(auto cb : cb_vec_) {
            cb->on_unpublish(app, streamname);
        }
    }
    return;
}

void media_stream_manager::set_hls_writer(av_writer_base* writer) {
    hls_writer_ = writer;
}

void media_stream_manager::set_rtc_writer(av_writer_base* writer) {
    r2r_writer_ = writer;
}

int media_stream_manager::writer_media_packet(MEDIA_PACKET_PTR pkt_ptr) {
    MEDIA_STREAM_PTR stream_ptr = add_publisher(pkt_ptr->key_);

    if (!stream_ptr) {
        log_errorf("fail to get stream key:%s", pkt_ptr->key_.c_str());
        return -1;
    }

    stream_ptr->cache_.insert_packet(pkt_ptr);
    std::vector<av_writer_base*> remove_list;

    for (auto item : stream_ptr->writer_map_) {
        auto writer = item.second;
        if (!writer->is_inited()) {
            writer->set_init_flag(true);
            if (stream_ptr->cache_.writer_gop(writer) < 0) {
                remove_list.push_back(writer);
            }
        } else {
            if (writer->write_packet(pkt_ptr) < 0) {
                std::string key_str = writer->get_key();
                std::string writerid = writer->get_writerid();
                log_warnf("writer send packet error, key:%s, id:%s",
                        key_str.c_str(), writerid.c_str());
                remove_list.push_back(writer);
            }
        }
    }

    if (media_stream_manager::r2r_writer_) {
        MEDIA_PACKET_PTR new_pkt_ptr = pkt_ptr->copy();
        media_stream_manager::r2r_writer_->write_packet(new_pkt_ptr);
    }

    if (media_stream_manager::hls_writer_) {
        MEDIA_PACKET_PTR new_pkt_ptr = pkt_ptr->copy();
        media_stream_manager::hls_writer_->write_packet(new_pkt_ptr);
    }
    
    for (auto write_p : remove_list) {
        remove_player(write_p);
    }

    return 0;
}
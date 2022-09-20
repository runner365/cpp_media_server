#include "data_buffer_pool.hpp"
#include "logger.hpp"
#include <sstream>

#define INIT_2K_NUMBER   20000
#define INIT_8K_NUMBER   10000
#define INIT_16K_NUMBER  3000
#define INIT_32K_NUMBER  3000
#define INIT_64K_NUMBER  3000
#define INIT_128K_NUMBER 1000

#define MAX_2K_NUMBER   40000
#define MAX_8K_NUMBER   20000
#define MAX_16K_NUMBER  8000
#define MAX_32K_NUMBER  8000
#define MAX_64K_NUMBER  8000
#define MAX_128K_NUMBER 5000

DataBufferPool DataBufferPool::s_pool;
extern uv_loop_t* get_global_io_context();

DataBufferPool& DataBufferPool::get_instance() {
    return s_pool;
}

DataBufferPool::DataBufferPool():timer_interface(get_global_io_context(), 30*1000)
{
    size_t i = 0;

    for (i = 0; i < INIT_2K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(2*1024);
        db_2k_list.emplace(std::move(db_ptr));
    }

    for (i = 0; i < INIT_8K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(8*1024);
        db_8k_list.emplace(std::move(db_ptr));
    }

    for (i = 0; i < INIT_16K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(16*1024);
        db_16k_list.emplace(std::move(db_ptr));
    }

    for (i = 0; i < INIT_32K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(32*1024);
        db_32k_list.emplace(std::move(db_ptr));
    }

    for (i = 0; i < INIT_64K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(64*1024);
        db_64k_list.emplace(std::move(db_ptr));
    }

    for (i = 0; i < INIT_128K_NUMBER; i++) {
        auto db_ptr = std::make_shared<data_buffer>(64*1024);
        db_128k_list.emplace(std::move(db_ptr));
    }
    start_timer();
}

DataBufferPool::~DataBufferPool()
{
}


std::shared_ptr<data_buffer> DataBufferPool::get_data_buffer(size_t len) {
    
    if (len <= 2*1024) {
        if (db_2k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(2*1024);
            used_2k_count_++;
            log_infof("make new 2k buffer");
            return db_ptr;
        }
        auto db_ptr = db_2k_list.front();
        db_2k_list.pop();
        used_2k_count_++;
        return db_ptr;
    } else if (len <= 8*1024) {
        if (db_8k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(8*1024);
            used_8k_count_++;
            log_infof("make new 8k buffer");
            return db_ptr;
        }
        auto db_ptr = db_8k_list.front();
        db_8k_list.pop();
        used_8k_count_++;
        return db_ptr;
    } else if (len <= 16*1024) {
        if (db_16k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(16*1024);
            used_16k_count_++;
            log_infof("make new 16k buffer");
            return db_ptr;
        }
        auto db_ptr = db_16k_list.front();
        db_16k_list.pop();
        used_16k_count_++;
        return db_ptr;
    } else if (len <= 32*1024) {
        if (db_32k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(32*1024);
            used_32k_count_++;
            log_infof("make new 32k buffer");
            return db_ptr;
        }
        auto db_ptr = db_32k_list.front();
        db_32k_list.pop();
        used_32k_count_++;
        return db_ptr;
    } else if (len <= 64*1024) {
        if (db_64k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(64*1024);
            used_64k_count_++;
            log_infof("make new 64k buffer");
            return db_ptr;
        }
        auto db_ptr = db_64k_list.front();
        db_64k_list.pop();
        used_64k_count_++;
        return db_ptr;
    } else if (len <= 128*1024) {
        if (db_128k_list.empty()) {
            auto db_ptr = std::make_shared<data_buffer>(128*1024);
            used_128k_count_++;
            log_infof("make new 128k buffer");
            return db_ptr;
        }
        auto db_ptr = db_128k_list.front();
        db_128k_list.pop();
        used_128k_count_++;
        return db_ptr;
    }

    return std::make_shared<data_buffer>(len);
}

void DataBufferPool::release(std::shared_ptr<data_buffer> db_ptr) {
    size_t len = db_ptr->buffer_size();

    db_ptr->reset();
    if (len <= 2*1024) {
        if (used_2k_count_-- < 0) {
            used_2k_count_ = 0;
            log_infof("release 2k buffer");
            return;
        }
        if (db_2k_list.size() >= MAX_2K_NUMBER) {
            log_infof("release 2k buffer");
            return;
        }
        db_2k_list.emplace(std::move(db_ptr));
    } else if (len <= 8*1024) {
        if (used_8k_count_-- < 0) {
            used_8k_count_ = 0;
            log_infof("release 8k buffer");
            return;
        }
        if (db_8k_list.size() >= MAX_8K_NUMBER) {
            log_infof("release 8k buffer");
            return;
        }
        db_8k_list.emplace(std::move(db_ptr));
    } else if (len <= 16*1024) {
        if (used_16k_count_-- < 0) {
            used_16k_count_ = 0;
            log_infof("release 16k buffer");
            return;
        }
        if (db_16k_list.size() >= MAX_16K_NUMBER) {
            log_infof("release 16k buffer");
            return;
        }
        db_16k_list.emplace(std::move(db_ptr));
    } else if (len <= 32*1024) {
        if (used_32k_count_-- < 0) {
            used_32k_count_ = 0;
            log_infof("release 32k buffer");
            return;
        }
        if (db_32k_list.size() >= MAX_32K_NUMBER) {
            log_infof("release 32k buffer");
            return;
        }
        db_32k_list.emplace(std::move(db_ptr));
    } else if (len <= 64*1024) {
        if (used_64k_count_-- < 0) {
            used_64k_count_ = 0;
            log_infof("release 64k buffer");
            return;
        }
        if (db_64k_list.size() >= MAX_64K_NUMBER) {
            log_infof("release 64k buffer");
            return;
        }
        db_64k_list.emplace(std::move(db_ptr));
    } else if (len <= 128*1024) {
        if (used_128k_count_-- < 0) {
            used_128k_count_ = 0;
            log_infof("release 128k buffer");
            return;
        }
        if (db_128k_list.size() >= MAX_128K_NUMBER) {
            return;
            log_infof("release 128k buffer");
        }
        db_128k_list.emplace(std::move(db_ptr));
    }
    db_ptr = nullptr;
}

void DataBufferPool::on_timer() {
    std::stringstream ss;

    ss << "data buffer 2k list size:" << db_2k_list.size() << ", ";
    ss << "data buffer 8k list size:" << db_8k_list.size() << ", ";
    ss << "data buffer 16k list size:" << db_16k_list.size() << ", ";
    ss << "data buffer 32k list size:" << db_32k_list.size() << ", ";
    ss << "data buffer 64k list size:" << db_64k_list.size() << ", ";
    ss << "data buffer 128k list size:" << db_128k_list.size();

    log_infof("%s", ss.str().c_str());
}
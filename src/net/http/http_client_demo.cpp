#include "http_client.hpp"
#include <sstream>
#include <uv.h>

class client : public http_client_callbackI
{
public:
    client(uv_loop_t* loop, const std::string& host, uint16_t port):host_(host)
                                                , port_(port)
                                                , loop_(loop)
    {
        
    }
    virtual ~client() {
    }

    void get(const std::string& subpath) {
        std::map<std::string, std::string> headers;
        if (hc_) {
            delete hc_;
            hc_ = nullptr;
        }
        hc_ = new http_client(loop_, host_, port_, this);

        hc_->get(subpath, headers);
    }

    void post(const std::string& subpath, const std::string& data) {
        std::map<std::string, std::string> headers;
        if (hc_) {
            delete hc_;
            hc_ = nullptr;
        }
        hc_ = new http_client(loop_, host_, port_, this);

        hc_->post(subpath, headers, data);
    }

private:
    virtual void on_http_read(int ret, std::shared_ptr<http_client_response> resp_ptr) override {
        if (ret < 0) {
            if (hc_) {
                hc_->close();
            }
            return;
        }
        std::string resp_data(resp_ptr->data_.data(), resp_ptr->data_.data_len());

        log_infof("http response:%s", resp_data.c_str());
    }

private:
    std::string host_;
    uint16_t port_ = 0;
    http_client* hc_ = nullptr;
    uv_loop_t* loop_;
};

int main(int argn, char** argv) {
    uint16_t port = 10020;
    std::string hostip = "127.0.0.1";
    uv_loop_t* loop = uv_default_loop();
    std::stringstream ss;

    try {
        client c(loop, hostip, port);

        c.get("/api/v1/getdemo");

        //ss << "{\r\n";
        //ss << "\"uid\": 123456,\r\n";
        //ss << "\"age\": 23,\r\n";
        //ss << "}\r\n";
        //c.post("/api/v1/postdemo", ss.str());
        
        uv_run(loop, UV_RUN_DEFAULT);

        log_infof("loop done...");
    }
    catch(const std::exception& e) {
        std::cerr << "http server exception:" << e.what() << "\r\n";
    }
    
    return 0;
}
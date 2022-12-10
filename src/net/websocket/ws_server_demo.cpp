#include "ws_server.hpp"
#include "ws_session.hpp"
#include "utils/logger.hpp"
#include <memory>

class ws_callback_implement : public websocket_server_callbackI
{
public:
    virtual void on_accept(std::shared_ptr<websocket_session> session) {
        log_infof("websocket session accept, uuid:%s", session->get_uuid().c_str());
    }

    virtual void on_read(std::shared_ptr<websocket_session> session, const char* data, size_t len) {
        log_infof("websocket session on read...");
    }

    virtual void on_close(std::shared_ptr<websocket_session> session) {
        log_infof("websocket session on close...");
    }
};

static std::unique_ptr<websocket_server> s_server_ptr;
static ws_callback_implement cb;

int main(int argn, char** argv)
{
    int port = 0;

    if (argn < 2) {
        log_errorf("input parameter error, please input: ./ws_server_demo 9090");
        return -1;
    }

    port = atoi(argv[1]);
    uv_loop_t* loop = uv_default_loop();

    log_infof("input tcp port:%d", port);
    try {
        s_server_ptr = std::make_unique<websocket_server>(loop, port, &cb);
        uv_run(loop, UV_RUN_DEFAULT);
    } catch(const std::exception& e) {
        std::cerr << "exception: " << e.what() << '\n';
        return -1;
    }
    
    return 0;
}
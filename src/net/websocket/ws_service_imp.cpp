#include "ws_service_imp.hpp"
#include "ws_server.hpp"
#include "wsimple/protoo_server.hpp"
#include "wsimple/flv_websocket.hpp"


ws_session_callback* create_websocket_implement(int imp_type, std::shared_ptr<websocket_session> session_ptr) {
    ws_session_callback* cb = nullptr;
    if (imp_type == WEBSOCKET_IMPLEMENT_FLV_TYPE) {
        cb = new flv_websocket();
    } else if (imp_type == WEBSOCKET_IMPLEMENT_PROTOO_TYPE) {
        cb = new protoo_server(session_ptr.get());
    }

    return cb;
}
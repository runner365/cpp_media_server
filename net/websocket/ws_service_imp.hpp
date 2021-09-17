#ifndef WS_SERVICE_IMP_HPP
#define WS_SERVICE_IMP_HPP
#include <memory>

#define WEBSOCKET_IMPLEMENT_FLV_TYPE    1
#define WEBSOCKET_IMPLEMENT_PROTOO_TYPE 2

class ws_session_callback;
class websocket_session;

ws_session_callback* create_websocket_implement(int imp_type, std::shared_ptr<websocket_session> session_ptr);

#endif
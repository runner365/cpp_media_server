#include "client.hpp"
#include "rtmp_player.hpp"
#include <thread>
#include <uv.h>

uv_loop_t* s_loop = uv_default_loop();
native_clientI* s_client = nullptr;
int main(int argn, char** argv) {
    int opt = 0;
    std::string input_url;
    
    Logger::get_instance()->set_filename("player.log");
    Logger::get_instance()->set_level(LOGGER_INFO_LEVEL);

    while ((opt = getopt(argn, argv, "i:")) != -1) {
      switch (opt) {
        case 'i':
          input_url = std::string(optarg);
          break;
        default:
          std::cerr << "Usage: " << argv[0]
               << " [-c config-file] "
               << std::endl;
          return -1;
      }
    }

    s_client = new rtmp_player(s_loop);
    s_client->open_url(input_url);

    std::thread th([&] {
        uv_run(s_loop, UV_RUN_DEFAULT);
    });

    s_client->run();
    return 0;
}
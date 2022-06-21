#include "rtmp_player.hpp"
#include "httpflv_player.hpp"
#include "utils/url_analyze.hpp"
#include "base_player.hpp"
#include <thread>
#include <uv.h>
#include <unistd.h>
#include <string>

uv_loop_t* s_loop = uv_default_loop();
base_player* s_client = nullptr;
int main(int argn, char** argv) {
    int opt = 0;
    std::string input_url;
    std::string scheme;
    
    Logger::get_instance()->set_filename("player.log");
    Logger::get_instance()->set_level(LOGGER_INFO_LEVEL);

    while ((opt = getopt(argn, argv, "i:")) != -1) {
      switch (opt) {
        case 'i':
          input_url = std::string(optarg);
          break;
        default:
          std::cerr << "Usage: " << argv[0]
               << " [-i live url] "
               << std::endl;
          return -1;
      }
    }

    int ret = get_scheme_by_url(input_url, scheme);
    if (ret != 0) {
      log_infof("get scheme error:%s", input_url.c_str());
      return ret;
    }

    if (scheme == "rtmp") {
      s_client = new rtmp_player(s_loop);
    } else if (scheme == "http") {
      s_client = new httpflv_player(s_loop);
    }
    
    s_client->open_url(input_url);

    std::thread th([&] {
        uv_run(s_loop, UV_RUN_DEFAULT);
    });

    s_client->run();
    return 0;
}
#include "hls_worker.hpp"

hls_worker* hls_worker::worker_ = nullptr;

hls_worker::hls_worker()
{
}

hls_worker::~hls_worker()
{
}

hls_worker* hls_worker::get_instance() {
    if (hls_worker::worker_ == nullptr) {
        hls_worker::worker_ = new hls_worker();
    }
    return hls_worker::worker_;
}

    
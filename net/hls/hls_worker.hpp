#ifndef HLS_WORKER_HPP
#define HLS_WORKER_HPP

class hls_worker
{
public:
    ~hls_worker();
    static hls_worker* get_instance();

protected:
    hls_worker();

private:
    static hls_worker* worker_;
};

#endif
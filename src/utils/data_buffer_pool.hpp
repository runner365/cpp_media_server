#ifndef DATA_BUFFER_POOL_HPP
#define DATA_BUFFER_POOL_HPP
#include "data_buffer.hpp"
#include "timer.hpp"
#include <queue>


class DataBufferPool : public timer_interface
{
public:
    static DataBufferPool& get_instance();
    virtual ~DataBufferPool();

public:
    std::shared_ptr<data_buffer> get_data_buffer(size_t len);
    void release(std::shared_ptr<data_buffer> db_ptr);

protected:
    virtual void on_timer();

private:
    DataBufferPool();

private:
    static DataBufferPool s_pool;

private:
    std::queue<std::shared_ptr<data_buffer>> db_2k_list;
    std::queue<std::shared_ptr<data_buffer>> db_8k_list;
    std::queue<std::shared_ptr<data_buffer>> db_16k_list;
    std::queue<std::shared_ptr<data_buffer>> db_32k_list;
    std::queue<std::shared_ptr<data_buffer>> db_64k_list;
    std::queue<std::shared_ptr<data_buffer>> db_128k_list;

private:
    int used_2k_count_   = 0;
    int used_8k_count_   = 0;
    int used_16k_count_  = 0;
    int used_32k_count_  = 0;
    int used_64k_count_  = 0;
    int used_128k_count_ = 0;
};

#endif
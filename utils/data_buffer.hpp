#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H
#include "logger.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>
#include <memory>

#define EXTRA_LEN (10*1024)

#define PRE_RESERVE_HEADER_SIZE 200

class data_buffer
{
public:
    data_buffer(size_t data_size = EXTRA_LEN);
    ~data_buffer();

public:
    int append_data(const char* input_data, size_t input_len);
    char* consume_data(int consume_len);
    void reset();

    char* data();
    size_t data_len();
    bool require(size_t len);

public:
    bool sent_flag_     = false;
    std::string dst_ip_;
    uint16_t    dst_port_ = 0;

private:
    char* buffer_       = nullptr;
    size_t buffer_size_ = 0;
    int data_len_       = 0;
    int start_          = PRE_RESERVE_HEADER_SIZE;
    int end_            = 0;
};

typedef std::shared_ptr<data_buffer> DATA_BUFFER_PTR;

#endif //DATA_BUFFER_H
#include "byte_stream.hpp"

double av_int2double(uint64_t i)
{
    union av_intfloat64 v;
    v.i = i;
    return v.f;
}

uint64_t av_double2int(double f)
{
    union av_intfloat64 v;
    v.f = f;
    return v.i;
}


uint64_t read_8bytes(const uint8_t* data) {
    uint64_t value = 0;
    uint8_t* output = (uint8_t*)&value;

    output[7] = *data++;
    output[6] = *data++;
    output[5] = *data++;
    output[4] = *data++;
    output[3] = *data++;
    output[2] = *data++;
    output[1] = *data++;
    output[0] = *data++;

    return value;
}

uint32_t read_4bytes(const uint8_t* data) {
    uint32_t value = 0;
    uint8_t* output = (uint8_t*)&value;

    output[3] = *data++;
    output[2] = *data++;
    output[1] = *data++;
    output[0] = *data++;

    return value;
}


uint32_t read_3bytes(const uint8_t* data) {
    uint32_t value = 0;
    uint8_t* output = (uint8_t*)&value;

    output[2] = *data++;
    output[1] = *data++;
    output[0] = *data++;

    return value;
}

uint16_t read_2bytes(const uint8_t* data) {
    uint16_t value = 0;
    uint8_t* output = (uint8_t*)&value;

    output[1] = *data++;
    output[0] = *data++;

    return value;
}

void write_8bytes(uint8_t* data, uint64_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[7];
    *p++ = pp[6];
    *p++ = pp[5];
    *p++ = pp[4];
    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void write_4bytes(uint8_t* data, uint32_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[3];
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void write_2bytes_be(uint8_t* data, uint32_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[0];
    *p++ = pp[1];
}

void write_4bytes_be(uint8_t* data, uint32_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[0];
    *p++ = pp[1];
    *p++ = pp[2];
    *p++ = pp[3];
}

void write_3bytes(uint8_t* data, uint32_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
}

void write_2bytes(uint8_t* data, uint16_t value) {
    uint8_t* p = data;
    uint8_t* pp = (uint8_t*)&value;

    *p++ = pp[1];
    *p++ = pp[0];
}

bool bytes_is_equal(const char* p1, const char* p2, size_t len) {
    for (size_t index = 0; index < len; index++) {
        if (p1[index] != p2[index]) {
            return false;
        }
    }
    return true;
}

uint16_t pad_to_4bytes(uint16_t size) {
    if (size & 0x03)
        return (size & 0xFFFC) + 4;
    else
        return size;
}

uint32_t pad_to_4bytes(uint32_t size) {
    if (size & 0x03)
        return (size & 0xFFFFFFFC) + 4;
    else
        return size;
}

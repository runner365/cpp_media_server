#ifndef BYTE_STREAM_HPP
#define BYTE_STREAM_HPP
#include <stdint.h>
#include <stddef.h>

/*
union av_intfloat32 {
    uint32_t i;
    float    f;
};
*/

/*
static uint32_t av_float2int(float f)
{
    union av_intfloat32 v;
    v.f = f;
    return v.i;
}
*/

double av_int2double(uint64_t i);
/**
 * Reinterpret a double as a 64-bit integer.
 */
uint64_t av_double2int(double f);

uint64_t read_8bytes(const uint8_t* data);
uint32_t read_4bytes(const uint8_t* data);
uint32_t read_3bytes(const uint8_t* data);
uint16_t read_2bytes(const uint8_t* data);

void write_8bytes(uint8_t* data, uint64_t value);
void write_4bytes(uint8_t* data, uint32_t value);
void write_2bytes_le(uint8_t* data, uint32_t value);
void write_4bytes_le(uint8_t* data, uint32_t value);
void write_3bytes(uint8_t* data, uint32_t value);
void write_2bytes(uint8_t* data, uint16_t value);

bool bytes_is_equal(const char* p1, const char* p2, size_t len);

uint16_t pad_to_4bytes(uint16_t size);
uint32_t pad_to_4bytes(uint32_t size);

#endif //BYTE_STREAM_HPP

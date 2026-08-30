#pragma once

#include <cstdint>

class VDsp {
public:
    static int32_t multiply_add(int32_t a, int32_t b, int32_t c);
    static int8_t multiply_i8(int8_t a, int8_t b);
    static int16_t multiply_i16(int16_t a, int16_t b);
    static int32_t multiply_i32(int32_t a, int32_t b);
};

#include "vfpga_dsp.h"

int32_t VDsp::multiply_add(int32_t a, int32_t b, int32_t c) {
    return a * b + c;
}

int8_t VDsp::multiply_i8(int8_t a, int8_t b) {
    return static_cast<int8_t>(a * b);
}

int16_t VDsp::multiply_i16(int16_t a, int16_t b) {
    return static_cast<int16_t>(a * b);
}

int32_t VDsp::multiply_i32(int32_t a, int32_t b) {
    return a * b;
}

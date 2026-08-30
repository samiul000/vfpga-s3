#pragma once

#include <cstdint>

struct VfpgaConfig {
    uint16_t lut_count = 0;
    uint16_t ff_count = 0;
    uint16_t bram_size = 0;
    uint16_t dsp_count = 0;
    uint16_t vio_count = 0;
};

#pragma once

#include <cstdint>
#include <cstddef>

struct VfpgaConfig {
    static constexpr uint32_t MAGIC = 0x56465047; // "VFPG"
    static constexpr uint16_t VERSION = 1;

    uint32_t magic = MAGIC;
    uint16_t version = VERSION;
    uint16_t lut_count = 0;
    uint16_t ff_count = 0;
    uint16_t bram_size = 0;
    uint16_t dsp_count = 0;
    uint16_t vio_count = 0;
    uint16_t checksum = 0;

    bool validate() const;
    uint16_t compute_checksum() const;
    bool save(const char *path) const;
    bool load(const char *path);
};

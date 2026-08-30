#pragma once

#include <cstdint>

class BoardProfile {
public:
    void detect();
    void print_diagnostics() const;

    const char* get_board_name() const;
    const char* get_chip_model() const;
    uint32_t get_chip_revision() const;
    uint32_t get_cpu_freq() const;
    uint32_t get_core_count() const;
    size_t get_flash_size() const;
    size_t get_psram_size() const;
};

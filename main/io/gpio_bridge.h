#pragma once

#include <cstdint>
#include "gpio_capability.h"
#include "board_profile.h"

class GpioBridge {
public:
    void begin(GpioCapability *cap, BoardProfile *board);
    bool map_input(uint16_t virtual_io, uint8_t physical_gpio);
    bool map_output(uint16_t virtual_io, uint8_t physical_gpio);
    bool unmap(uint16_t virtual_io);
    bool validate_mapping(uint16_t virtual_io, uint8_t physical_gpio);
    void sample_inputs();
    void commit_outputs();

private:
    static constexpr size_t MAX_VIO = 256;
    struct Mapping { uint8_t gpio; bool is_input; bool allocated; };
    Mapping mappings_[MAX_VIO]{};
    GpioCapability *cap_ = nullptr;
    BoardProfile *board_ = nullptr;
};

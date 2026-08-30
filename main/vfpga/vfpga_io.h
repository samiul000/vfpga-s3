#pragma once

#include <cstdint>
#include <cstddef>

using VSignal = uint32_t;

class VFpgaIo {
public:
    void init(size_t input_count, size_t output_count);
    VSignal read_input(uint16_t id) const;
    void write_input(uint16_t id, VSignal value);
    VSignal read_output(uint16_t id) const;
    void write_output(uint16_t id, VSignal value);

private:
    VSignal *inputs_ = nullptr;
    VSignal *outputs_ = nullptr;
    size_t input_count_ = 0;
    size_t output_count_ = 0;
};

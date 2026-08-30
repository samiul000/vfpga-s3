#pragma once

#include <cstdint>

using VSignal = uint32_t;

class VLut4 {
public:
    void configure(uint16_t truth_table);
    VSignal evaluate(VSignal a, VSignal b, VSignal c, VSignal d);

private:
    uint16_t truth_table_ = 0;
};

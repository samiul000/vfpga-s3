#pragma once

#include <cstdint>

using VSignal = uint32_t;

class VMux {
public:
    static VSignal mux2(VSignal a, VSignal b, VSignal sel);
    static VSignal mux4(VSignal a, VSignal b, VSignal c, VSignal d, VSignal sel);
};

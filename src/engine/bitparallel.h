#pragma once

#include <cstdint>

using VSignal = uint32_t;

class BitParallel {
public:
    static VSignal op_and(VSignal a, VSignal b);
    static VSignal op_or(VSignal a, VSignal b);
    static VSignal op_xor(VSignal a, VSignal b);
    static VSignal op_not(VSignal a);
    static VSignal op_nand(VSignal a, VSignal b);
    static VSignal op_nor(VSignal a, VSignal b);
    static VSignal op_xnor(VSignal a, VSignal b);
    static VSignal op_mux(VSignal a, VSignal b, VSignal sel);
};

#pragma once

#include <cstdint>

using VSignal = uint32_t;

class VLut4 {
public:
    void configure(uint16_t truth_table);
    VSignal evaluate(VSignal a, VSignal b, VSignal c, VSignal d) const;

    uint16_t truth_table() const { return truth_table_; }
    /*
       precomputed branchless lookup table
       lut_table_[k] = all-1s if truth_table bit k is set, all-0s otherwise
       eliminates branch mispredictions in evaluate()
    */
    alignas(16) uint32_t lut_table_[16]{};

private:
    uint16_t truth_table_ = 0;
};

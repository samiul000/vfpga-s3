#include "vfpga_lut.h"

void VLut4::configure(uint16_t truth_table) {
    truth_table_ = truth_table;
}

VSignal VLut4::evaluate(VSignal a, VSignal b, VSignal c, VSignal d) {
    VSignal result = 0;
    for (int i = 0; i < 32; ++i) {
        uint8_t idx = 0;
        if (a & (1U << i)) idx |= 0x01;
        if (b & (1U << i)) idx |= 0x02;
        if (c & (1U << i)) idx |= 0x04;
        if (d & (1U << i)) idx |= 0x08;
        if (truth_table_ & (1U << idx)) {
            result |= (1U << i);
        }
    }
    return result;
}

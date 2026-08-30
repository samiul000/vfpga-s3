#include "vfpga_ff.h"

void VFlipFlop::reset() {
    current_state_ = 0;
    next_state_ = 0;
}

void VFlipFlop::clock_edge(VSignal d, bool enable) {
    if (enable) {
        current_state_ = d;
    }
}

VSignal VFlipFlop::output() const {
    return current_state_;
}

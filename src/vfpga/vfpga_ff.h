#pragma once

#include <cstdint>

using VSignal = uint32_t;

class VFlipFlop {
public:
    void reset();
    void clock_edge(VSignal d, bool enable);
    VSignal output() const;

private:
    VSignal current_state_ = 0;
    VSignal next_state_ = 0;
};

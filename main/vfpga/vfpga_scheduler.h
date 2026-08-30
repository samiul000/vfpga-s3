#pragma once

#include <cstdint>
#include <cstddef>

using VSignal = uint32_t;

class VFpgaScheduler {
public:
    void init(size_t signal_count);
    void route(uint16_t src_id, uint16_t dst_id);
    VSignal get_signal(uint16_t id) const;
    void set_signal(uint16_t id, VSignal value);
    void evaluate();

private:
    static constexpr size_t MAX_SIGNALS = 256;
    VSignal signals_[MAX_SIGNALS]{};
    struct Route { uint16_t src; uint16_t dst; };
    Route routes_[MAX_SIGNALS]{};
    size_t route_count_ = 0;
    size_t signal_count_ = 0;
};

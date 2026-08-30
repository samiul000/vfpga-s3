#include "vfpga_scheduler.h"

void VFpgaScheduler::init(size_t signal_count) {
    signal_count_ = signal_count;
    route_count_ = 0;
    for (size_t i = 0; i < MAX_SIGNALS; ++i) signals_[i] = 0;
}

void VFpgaScheduler::route(uint16_t src_id, uint16_t dst_id) {
    if (route_count_ < MAX_SIGNALS) {
        routes_[route_count_++] = {src_id, dst_id};
    }
}

VSignal VFpgaScheduler::get_signal(uint16_t id) const {
    if (id < MAX_SIGNALS) return signals_[id];
    return 0;
}

void VFpgaScheduler::set_signal(uint16_t id, VSignal value) {
    if (id < MAX_SIGNALS) signals_[id] = value;
}

void VFpgaScheduler::evaluate() {
    for (size_t i = 0; i < route_count_; ++i) {
        signals_[routes_[i].dst] = signals_[routes_[i].src];
    }
}

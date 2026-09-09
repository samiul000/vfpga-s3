#pragma once
// Host-only change-based signal tracer. Records (time, net, value)
// only when a watched net's value changes. Header-only.
// no timestamps per timestep, no per-signal classes
#include <cstdint>
#include <vector>

#include "sim.h"

struct TraceSample {
    SimTime t = 0;
    uint16_t net = 0;
    uint32_t value = 0;
};

class Tracer {
public:
    void watch(uint16_t net) {
        for (size_t i = 0; i < nets_.size(); ++i)
            if (nets_[i] == net) return;
        nets_.push_back(net);
        last_.push_back(0);
        has_last_.push_back(false);
    }

    void watch_all(const Simulator &sim) {
        for (size_t i = 0; i < sim.signal_count(); ++i)
            watch(sim.signal(i).net_id);
    }

    // Snapshot watched nets at time t; records only changes.
    void record(SimTime t, const Simulator &sim) {
        for (size_t i = 0; i < nets_.size(); ++i) {
            uint32_t v = sim.read(nets_[i]);
            if (!has_last_[i] || v != last_[i]) {
                samples_.push_back({t, nets_[i], v});
                last_[i] = v;
                has_last_[i] = true;
            }
        }
    }

    const std::vector<TraceSample> &samples() const { return samples_; }
    const std::vector<uint16_t> &watched() const { return nets_; }
    void clear() { samples_.clear(); }

private:
    std::vector<uint16_t> nets_;
    std::vector<uint32_t> last_;
    std::vector<bool> has_last_;
    std::vector<TraceSample> samples_;
};

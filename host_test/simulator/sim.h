#pragma once
// Host-only VFPGA simulator core (Stage 2). No ESP-IDF dependencies.
// Mirrors src/vfpga/vfpga_core.cpp semantics bit-exactly:
//  - LUT4 bit-parallel eval, missing inputs padded with net 0 (firmware parity)
//  - FF: Q = D on clock()
//  - constants loaded from MappedConfig, reset() zeroes everything
// no Clock/EventQueue classes yet; a SimTime counter + advance()
// covers stages 2-3. Add scheduler classes when the TB executor (stage 9) needs them.
#include <cstdint>
#include <string>
#include <vector>

#include "mapper.h"
#include "netlist.h"

using SimTime = uint64_t;  // logical nanoseconds, NOT ESP32 cycles (§9)

struct SimSignal {
    uint16_t net_id = 0;
    std::string name;
};

class Simulator {
public:
    void load(const Netlist &netlist, const MappedConfig &cfg);
    void reset();

    void write_input(uint16_t net, uint32_t value);
    uint32_t read(uint16_t net) const;
    int16_t resolve(const std::string &name) const { return netlist_->resolve(name); }

    // Single ordered LUT pass (firmware parity: vfpga_core run_cycles =
    // evaluate_combinational + clock; nets hold state between passes).
    void eval_combinational();
    void clock();  // FF D -> Q (no-op when D and Q share a net, kept for fidelity)
    void step() { eval_combinational(); clock(); }

    SimTime time_ns() const { return time_ns_; }
    void advance(SimTime dt) { time_ns_ += dt; }

    size_t signal_count() const { return signals_.size(); }
    const SimSignal &signal(size_t i) const { return signals_[i]; }

private:
    static uint32_t eval_lut(uint16_t tt, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

    const Netlist *netlist_ = nullptr;
    const MappedConfig *cfg_ = nullptr;
    std::vector<uint32_t> values_;
    std::vector<SimSignal> signals_;
    SimTime time_ns_ = 0;
};

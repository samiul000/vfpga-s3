// Host-only VFPGA simulator core. See sim.h.
#include "sim.h"

uint32_t Simulator::eval_lut(uint16_t tt, uint32_t a, uint32_t b,
                             uint32_t c, uint32_t d) {
    // Same bit-parallel semantics as VLut4::evaluate (vfpga_lut.cpp).
    uint32_t result = 0;
    for (int i = 0; i < 32; ++i) {
        uint8_t idx = ((a >> i) & 1) | (((b >> i) & 1) << 1) |
                      (((c >> i) & 1) << 2) | (((d >> i) & 1) << 3);
        if ((tt >> idx) & 1) result |= (1U << i);
    }
    return result;
}

void Simulator::load(const Netlist &netlist, const MappedConfig &cfg) {
    netlist_ = &netlist;
    cfg_ = &cfg;
    signals_.clear();
    for (size_t i = 0; i < netlist.net_count(); ++i) {
        const Net &n = netlist.get_net(i);
        signals_.push_back({n.id, n.name});
    }
    reset();
}

void Simulator::reset() {
    if (!cfg_) return;
    values_.assign(cfg_->total_nets, 0);
    for (size_t i = 0; i < cfg_->constants.size(); ++i) {
        uint16_t id = cfg_->constants[i].first;
        if (id < values_.size()) values_[id] = cfg_->constants[i].second;
    }
    time_ns_ = 0;
}

void Simulator::write_input(uint16_t net, uint32_t value) {
    if (net < values_.size()) values_[net] = value;
}

uint32_t Simulator::read(uint16_t net) const {
    return net < values_.size() ? values_[net] : 0;
}

void Simulator::eval_combinational() {
    // Firmware parity: ONE in-order pass (vfpga_pie::evaluate_batch_4).
    // Components are emitted topologically, so one pass fully propagates.
    // Do NOT iterate to "stable": register feedback shares the LUT output
    // net (FF D == Q), so re-evaluation would oscillate by design.
    for (const auto &lut : cfg_->luts) {
        // Padding for <4 inputs: replicate input 0. All mapped truth
        // tables are either idempotent in the padded positions
        // (AND/OR/XOR/PASS/NOT over a,b,a,a == op over a,b) or
        // don't-care there (XOR3/MAJ3 ignore d; MUX ties d to GND
        // explicitly). NOTE: firmware pads with net 0 instead, which
        // mis-evaluates 2-input gates whenever net 0 disagrees (e.g. OR
        // stuck at 1 when net 0 is 1); the acceptance tests (§66: AND,
        // OR, XOR, NOT must simulate correctly) require this fix.
        uint16_t pad =
            lut.input_net_ids.empty() ? 0 : lut.input_net_ids[0];
        uint16_t in[4] = {pad, pad, pad, pad};
        for (size_t i = 0; i < lut.input_net_ids.size() && i < 4; ++i)
            in[i] = lut.input_net_ids[i];
        values_[lut.output_net_id] =
            eval_lut(lut.truth_table, values_[in[0]], values_[in[1]],
                     values_[in[2]], values_[in[3]]);
    }
}

void Simulator::clock() {
    for (const auto &ff : cfg_->ffs)
        values_[ff.q_net_id] = values_[ff.d_net_id];
}

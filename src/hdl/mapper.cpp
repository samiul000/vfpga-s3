#include "mapper.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char *TAG = "mapper";

static uint32_t parse_hdl_constant(const std::string &name) {
    if (name.size() >= 3 && name[1] == '\'') {
        char base = name[2];
        const char *val = name.c_str() + 3;
        if (base == 'h' || base == 'H') return (uint32_t)strtol(val, nullptr, 16);
        if (base == 'b' || base == 'B') return (uint32_t)strtol(val, nullptr, 2);
        return (uint32_t)strtol(val, nullptr, 10);
    }
    if (name.size() >= 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X'))
        return (uint32_t)strtol(name.c_str(), nullptr, 16);
    bool all_digits = true;
    for (char c : name) if (!isdigit((unsigned char)c)) { all_digits = false; break; }
    if (all_digits && !name.empty()) return (uint32_t)strtol(name.c_str(), nullptr, 10);
    return 0;
}

static bool is_hdl_constant(const std::string &name) {
    if (name.size() >= 3 && name[1] == '\'') {
        char base = name[2];
        return base == 'h' || base == 'H' || base == 'b' || base == 'B' || isdigit((unsigned char)base);
    }
    if (name.size() >= 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) return true;
    bool all_digits = true;
    for (char c : name) if (!isdigit((unsigned char)c)) { all_digits = false; break; }
    return all_digits && !name.empty();
}

uint16_t Mapper::truth_table_for_op(const std::string &op) const {
    // idx bits: a(bit0) | b(bit1) | c(bit2) | d(bit3)
    // AND: only 1111->1 = bit15 = 0x8000
    if (op == "&") return 0x8000;
    // OR: all except 0000->0 = bits 1-15 = 0xFFFE
    if (op == "|") return 0xFFFE;
    // XOR(a,b): bits 1,2,5,6,9,10,13,14 = 0x6666
    if (op == "^") return 0x6666;
    // PASS(a): bits where a=1 = 0xAAAA
    if (op == "PASS") return 0xAAAA;
    // ADD: same as XOR for sum bit (no carry)
    if (op == "+") return 0x6666;
    if (op == "==") return 0x8000;
    if (op == "!=") return 0x7FFF;
    // MUX: a=then(bit0), b=else(bit1), c=cond(bit2)
    // c=0 -> a(then), c=1 -> b(else)
    if (op == "MUX") return 0x00CA;
    // NOT(a): a=bit0, output=!a for all b,c,d combos: bits 0,2,4,6,8,10,12,14 = 0x5555
    if (op == "!") return 0x5555;
    ESP_LOGW(TAG, "Unknown op '%s', using AND", op.c_str());
    return 0x8000;
}

MappedConfig Mapper::map_to_luts(const Netlist &netlist) {
    MappedConfig cfg;
    cfg.total_nets = netlist.next_net_id();

    for (const auto &name : netlist.input_names()) {
        int16_t id = netlist.resolve(name);
        if (id >= 0) cfg.input_net_ids.push_back(static_cast<uint16_t>(id));
    }
    for (const auto &name : netlist.output_names()) {
        int16_t id = netlist.resolve(name);
        if (id >= 0) cfg.output_net_ids.push_back(static_cast<uint16_t>(id));
    }

    for (size_t i = 0; i < netlist.component_count(); ++i) {
        const auto &comp = netlist.get_component(i);

        if (comp.type == NetlistComponent::Type::LUT4) {
            MappedLut lut;
            lut.id = comp.id;
            lut.truth_table = truth_table_for_op(comp.op);
            lut.input_net_ids = comp.inputs;
            lut.output_net_id = comp.output;
            cfg.luts.push_back(lut);
        } else if (comp.type == NetlistComponent::Type::FF) {
            MappedFf ff;
            ff.id = comp.id;
            ff.d_net_id = comp.inputs.empty() ? comp.output : comp.inputs[0];
            ff.q_net_id = comp.output;
            cfg.ffs.push_back(ff);
        }
    }

    for (size_t i = 0; i < netlist.net_count(); ++i) {
        const auto &net = netlist.get_net(i);
        if (is_hdl_constant(net.name)) {
            cfg.constants.push_back({net.id, parse_hdl_constant(net.name)});
        }
    }

    ESP_LOGI(TAG, "Mapped: %zu LUTs, %zu FFs, %zu nets, %zu constants",
             cfg.luts.size(), cfg.ffs.size(), cfg.total_nets, cfg.constants.size());
    return cfg;
}

size_t Mapper::lut_count() const { return 0; }

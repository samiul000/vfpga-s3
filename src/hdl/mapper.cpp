#include "mapper.h"
#include "esp_log.h"
#include <cstdio>

static const char *TAG = "mapper";

uint16_t Mapper::truth_table_for_op(const std::string &op) const {
    if (op == "&") return 0x8000;
    if (op == "|") return 0xFE00;
    if (op == "^") return 0x6969;
    if (op == "PASS") return 0xAAAA;
    if (op == "+") return 0x6969;
    if (op == "==") return 0x8000;
    if (op == "!=") return 0x7FFF;
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

    ESP_LOGI(TAG, "Mapped: %zu LUTs, %zu FFs, %zu nets",
             cfg.luts.size(), cfg.ffs.size(), cfg.total_nets);
    return cfg;
}

size_t Mapper::lut_count() const { return 0; }

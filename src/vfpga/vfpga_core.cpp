#include "vfpga_core.h"
#include "mapper.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "vfpga_core";

void VFpgaCore::initialize() {
    reset();
    ESP_LOGI(TAG, "VFPGA core initialized (max %d signals, %d LUTs, %d FFs)",
             MAX_SIGNALS, MAX_LUTS, MAX_FFS);
}

void VFpgaCore::reset() {
    memset(signals_, 0, sizeof(signals_));
    for (size_t i = 0; i < MAX_LUTS; ++i) luts_[i] = {};
    for (size_t i = 0; i < MAX_FFS; ++i) ffs_[i] = {};
    lut_count_ = 0;
    ff_count_ = 0;
}

void VFpgaCore::load_config(const MappedConfig &cfg) {
    reset();

    for (const auto &lut : cfg.luts) {
        if (lut_count_ >= MAX_LUTS) { ESP_LOGE(TAG, "Too many LUTs"); break; }
        CoreLut &cl = luts_[lut_count_];
        cl.lut.configure(lut.truth_table);
        for (size_t i = 0; i < lut.input_net_ids.size() && i < 4; ++i) {
            cl.input_ids[i] = lut.input_net_ids[i];
        }
        cl.output_id = lut.output_net_id;
        lut_count_++;
    }

    for (const auto &ff : cfg.ffs) {
        if (ff_count_ >= MAX_FFS) { ESP_LOGE(TAG, "Too many FFs"); break; }
        CoreFf &cff = ffs_[ff_count_];
        cff.ff.reset();
        cff.d_id = ff.d_net_id;
        cff.q_id = ff.q_net_id;
        ff_count_++;
    }

    ESP_LOGI(TAG, "Loaded config: %zu LUTs, %zu FFs, %zu nets",
             lut_count_, ff_count_, cfg.total_nets);
}

void VFpgaCore::evaluate_combinational() {
    for (size_t i = 0; i < lut_count_; ++i) {
        const CoreLut &cl = luts_[i];
        VSignal a = signals_[cl.input_ids[0]];
        VSignal b = signals_[cl.input_ids[1]];
        VSignal c = signals_[cl.input_ids[2]];
        VSignal d = signals_[cl.input_ids[3]];
        signals_[cl.output_id] = cl.lut.evaluate(a, b, c, d);
    }
}

void VFpgaCore::clock() {
    for (size_t i = 0; i < ff_count_; ++i) {
        CoreFf &cff = ffs_[i];
        VSignal d_val = signals_[cff.d_id];
        cff.ff.clock_edge(d_val, true);
        signals_[cff.q_id] = cff.ff.output();
    }
}

void VFpgaCore::run_cycles(uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; ++i) {
        evaluate_combinational();
        clock();
    }
}

VSignal VFpgaCore::read_input(uint16_t id) {
    if (id < MAX_SIGNALS) return signals_[id];
    return 0;
}

void VFpgaCore::write_input(uint16_t id, VSignal value) {
    if (id < MAX_SIGNALS) signals_[id] = value;
}

VSignal VFpgaCore::read_output(uint16_t id) {
    if (id < MAX_SIGNALS) return signals_[id];
    return 0;
}

VSignal VFpgaCore::read_signal(uint16_t id) const {
    if (id < MAX_SIGNALS) return signals_[id];
    return 0;
}

size_t VFpgaCore::lut_count() const { return lut_count_; }
size_t VFpgaCore::ff_count() const { return ff_count_; }

#include "vfpga_core.h"
#include "vfpga_pie.h"
#include "mapper.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstring>

static const char *TAG = "vfpga_core";

// ponytail: 16-byte alignment for Xtensa PIE SIMD registers
static constexpr uint32_t ALIGN = 16;
// ponytail: use PSRAM for large arrays (internal SRAM too fragmented for 4K LUTs)
static constexpr uint32_t MALLOC_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

VFpgaCore::~VFpgaCore() { destroy(); }

void VFpgaCore::init(size_t max_signals, size_t max_luts, size_t max_ffs) {
    destroy();
    max_signals_ = max_signals;
    max_luts_ = max_luts;
    max_ffs_ = max_ffs;

    signals_ = (VSignal *)heap_caps_aligned_alloc(ALIGN, max_signals_ * sizeof(VSignal), MALLOC_CAPS);
    luts_ = (CoreLut *)heap_caps_aligned_alloc(ALIGN, max_luts_ * sizeof(CoreLut), MALLOC_CAPS);
    ffs_ = max_ffs_ ? (CoreFf *)heap_caps_aligned_alloc(ALIGN, max_ffs_ * sizeof(CoreFf), MALLOC_CAPS) : nullptr;

    if ((max_signals_ && !signals_) || (max_luts_ && !luts_) || (max_ffs_ && !ffs_)) {
        ESP_LOGE(TAG, "Failed to allocate: signals=%p luts=%p ffs=%p",
                 signals_, luts_, ffs_);
        destroy();
        return;
    }

    initialized_ = true;
    reset();
    ESP_LOGI(TAG, "VFPGA core initialized (%zu signals, %zu LUTs, %zu FFs, %zu KB)",
             max_signals_, max_luts_, max_ffs_,
             (max_signals_ * sizeof(VSignal) + max_luts_ * sizeof(CoreLut) + max_ffs_ * sizeof(CoreFf)) / 1024);
}

void VFpgaCore::destroy() {
    if (signals_) { heap_caps_free(signals_); signals_ = nullptr; }
    if (luts_) { heap_caps_free(luts_); luts_ = nullptr; }
    if (ffs_) { heap_caps_free(ffs_); ffs_ = nullptr; }
    max_signals_ = max_luts_ = max_ffs_ = 0;
    lut_count_ = ff_count_ = 0;
    initialized_ = false;
}

void VFpgaCore::reset() {
    if (!initialized_) return;
    memset(signals_, 0, max_signals_ * sizeof(VSignal));
    for (size_t i = 0; i < max_luts_; ++i) luts_[i] = {};
    if (ffs_) for (size_t i = 0; i < max_ffs_; ++i) ffs_[i] = {};
    lut_count_ = 0;
    ff_count_ = 0;
}

void VFpgaCore::load_config(const MappedConfig &cfg) {
    reset();

    for (const auto &lut : cfg.luts) {
        if (lut_count_ >= max_luts_) { ESP_LOGE(TAG, "Too many LUTs"); break; }
        CoreLut &cl = luts_[lut_count_];
        cl.lut.configure(lut.truth_table);
        for (size_t i = 0; i < lut.input_net_ids.size() && i < 4; ++i) {
            cl.input_ids[i] = lut.input_net_ids[i];
        }
        cl.output_id = lut.output_net_id;
        lut_count_++;
    }

    for (const auto &ff : cfg.ffs) {
        if (ff_count_ >= max_ffs_) { ESP_LOGE(TAG, "Too many FFs"); break; }
        CoreFf &cff = ffs_[ff_count_];
        cff.ff.reset();
        cff.d_id = ff.d_net_id;
        cff.q_id = ff.q_net_id;
        ff_count_++;
    }

    for (const auto &[net_id, value] : cfg.constants) {
        if (net_id < max_signals_) {
            signals_[net_id] = value;
        }
    }

    ESP_LOGI(TAG, "Loaded: %zu LUTs, %zu FFs, %zu constants",
             lut_count_, ff_count_, cfg.constants.size());
}

void VFpgaCore::evaluate_combinational() {
    // ponytail: PIE batch evaluation — process 4 LUTs at a time
    vfpga_pie::evaluate_batch_4(luts_, signals_, signals_, lut_count_);
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
    if (id < max_signals_) return signals_[id];
    return 0;
}

void VFpgaCore::write_input(uint16_t id, VSignal value) {
    if (id < max_signals_) signals_[id] = value;
}

VSignal VFpgaCore::read_output(uint16_t id) {
    if (id < max_signals_) return signals_[id];
    return 0;
}

VSignal VFpgaCore::read_signal(uint16_t id) const {
    if (id < max_signals_) return signals_[id];
    return 0;
}

size_t VFpgaCore::lut_count() const { return lut_count_; }
size_t VFpgaCore::ff_count() const { return ff_count_; }
size_t VFpgaCore::max_signals() const { return max_signals_; }
size_t VFpgaCore::max_luts() const { return max_luts_; }

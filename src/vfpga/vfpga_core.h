#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "vfpga_lut.h"
#include "vfpga_ff.h"

using VSignal = uint32_t;

struct CoreLut {
    VLut4 lut;
    uint16_t input_ids[4]{0, 0, 0, 0};
    uint16_t output_id;
};

struct CoreFf {
    VFlipFlop ff;
    uint16_t d_id;
    uint16_t q_id;
};

struct MappedConfig;

class VFpgaCore {
public:
    VFpgaCore() = default;
    ~VFpgaCore();
    VFpgaCore(const VFpgaCore&) = delete;
    VFpgaCore& operator=(const VFpgaCore&) = delete;

    void init(size_t max_signals = 4096, size_t max_luts = 4096, size_t max_ffs = 4096);
    void destroy();
    void reset();
    void load_config(const MappedConfig &cfg);

    void evaluate_combinational();
    void clock();
    void run_cycles(uint32_t cycles);

    VSignal read_input(uint16_t id);
    void write_input(uint16_t id, VSignal value);
    VSignal read_output(uint16_t id);
    VSignal read_signal(uint16_t id) const;

    size_t lut_count() const;
    size_t ff_count() const;
    size_t max_signals() const;
    size_t max_luts() const;

private:
    VSignal *signals_ = nullptr;
    CoreLut *luts_ = nullptr;
    CoreFf *ffs_ = nullptr;
    size_t max_signals_ = 0;
    size_t max_luts_ = 0;
    size_t max_ffs_ = 0;
    size_t lut_count_ = 0;
    size_t ff_count_ = 0;
    bool initialized_ = false;
};

#pragma once

#include <cstdint>
#include <cstddef>

using VSignal = uint32_t;

class VFpgaCore {
public:
    void initialize();
    void reset();

    void evaluate_combinational();
    void clock();
    void run_cycles(uint32_t cycles);

    VSignal read_input(uint16_t id);
    void write_input(uint16_t id, VSignal value);
    VSignal read_output(uint16_t id);

private:
    static constexpr size_t MAX_SIGNALS = 256;
    static constexpr size_t MAX_LUTS = 64;
    static constexpr size_t MAX_FFS = 64;

    VSignal input_state[MAX_SIGNALS]{};
    VSignal output_state[MAX_SIGNALS]{};
    VSignal internal_state[MAX_SIGNALS]{};
};

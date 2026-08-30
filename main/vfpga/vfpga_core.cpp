#include "vfpga_core.h"

void VFpgaCore::initialize() {
    reset();
}

void VFpgaCore::reset() {
    for (size_t i = 0; i < MAX_SIGNALS; ++i) {
        input_state[i] = 0;
        output_state[i] = 0;
        internal_state[i] = 0;
    }
}

void VFpgaCore::evaluate_combinational() {
    // TODO: evaluate LUTs and combinational logic
}

void VFpgaCore::clock() {
    // TODO: commit next state on clock edge
}

void VFpgaCore::run_cycles(uint32_t cycles) {
    for (uint32_t i = 0; i < cycles; ++i) {
        evaluate_combinational();
        clock();
    }
}

VSignal VFpgaCore::read_input(uint16_t id) {
    if (id < MAX_SIGNALS) return input_state[id];
    return 0;
}

void VFpgaCore::write_input(uint16_t id, VSignal value) {
    if (id < MAX_SIGNALS) input_state[id] = value;
}

VSignal VFpgaCore::read_output(uint16_t id) {
    if (id < MAX_SIGNALS) return output_state[id];
    return 0;
}

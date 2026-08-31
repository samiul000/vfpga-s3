#pragma once

#include <cstdint>
#include <cstddef>
#include "riscv_memory.h"

class RiscvCpu {
public:
    void reset();
    void step();
    void run(uint32_t instructions);
    void load_program(uint32_t addr, const uint32_t *data, size_t count);
    uint32_t get_pc() const;
    uint32_t get_reg(uint8_t idx) const;

private:
    void execute(uint32_t instruction);

    uint32_t pc_ = 0;
    uint32_t regs_[32]{};
    RiscvMemory mem_;
};

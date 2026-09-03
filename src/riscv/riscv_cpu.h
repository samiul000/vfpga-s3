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
    void set_io_handler(void *ctx,
        uint32_t (*read)(void *ctx, uint32_t addr),
        void (*write)(void *ctx, uint32_t addr, uint32_t data));
    uint32_t get_pc() const;
    uint32_t get_reg(uint8_t idx) const;
    // Debug/test accessor: read emulated RAM (used by ISA coverage demo).
    uint32_t read_mem_word(uint32_t addr) const { return mem_.read_word(addr); }

private:
    void execute(uint32_t instruction);

    uint32_t pc_ = 0;
    uint32_t regs_[32]{};
    RiscvMemory mem_;
};

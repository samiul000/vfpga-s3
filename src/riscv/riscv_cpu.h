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
    uint32_t get_csr(uint16_t addr) const;
    void set_csr(uint16_t addr, uint32_t val);
    // Debug/test accessor: read emulated RAM (used by ISA coverage demo).
    uint32_t read_mem_word(uint32_t addr) const { return mem_.read_word(addr); }

private:
    void execute(uint32_t instruction);

    // CSR index helper: maps 12-bit CSR address to array slot (-1 = unsupported).
    int csr_index(uint16_t addr) const;

    uint32_t pc_ = 0;
    uint32_t regs_[32]{};
    RiscvMemory mem_;

    // Minimal CSR file (educational). Indices:
    //   0 = mstatus (0x300), 1 = mie (0x304), 2 = mscratch (0x340),
    //   3 = mepc (0x341), 4 = mcause (0x342), 5 = mip (0x344).
    static constexpr int kCsrCount = 6;
    uint32_t csr_[kCsrCount]{};
};

#include "riscv_cpu.h"

void RiscvCpu::reset() {
    pc_ = 0;
    for (int i = 0; i < 32; ++i) regs_[i] = 0;
    mem_.init(64 * 1024);
}

void RiscvCpu::step() {
    uint32_t inst = mem_.read_word(pc_);
    execute(inst);
    pc_ += 4;
}

void RiscvCpu::run(uint32_t instructions) {
    for (uint32_t i = 0; i < instructions; ++i) step();
}

uint32_t RiscvCpu::get_pc() const { return pc_; }

uint32_t RiscvCpu::get_reg(uint8_t idx) const {
    if (idx < 32) return regs_[idx];
    return 0;
}

void RiscvCpu::execute(uint32_t instruction) {
    uint8_t opcode = instruction & 0x7F;
    if (opcode == 0x33) {
        uint8_t rd = (instruction >> 7) & 0x1F;
        uint8_t rs1 = (instruction >> 15) & 0x1F;
        uint8_t rs2 = (instruction >> 20) & 0x1F;
        uint8_t funct3 = (instruction >> 12) & 0x7;
        if (rd == 0) return;
        switch (funct3) {
            case 0x0: regs_[rd] = regs_[rs1] + regs_[rs2]; break;
            case 0x2: regs_[rd] = regs_[rs1] - regs_[rs2]; break;
            case 0x7: regs_[rd] = regs_[rs1] & regs_[rs2]; break;
            case 0x6: regs_[rd] = regs_[rs1] | regs_[rs2]; break;
            case 0x4: regs_[rd] = regs_[rs1] ^ regs_[rs2]; break;
            default: break;
        }
    }
}

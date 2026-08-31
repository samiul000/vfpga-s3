#include "riscv_cpu.h"
#include "esp_log.h"

static const char *TAG = "riscv";

void RiscvCpu::reset() {
    pc_ = 0;
    for (int i = 0; i < 32; ++i) regs_[i] = 0;
    mem_.init(64 * 1024);
    ESP_LOGI(TAG, "RISC-V CPU reset (32 regs, 64KB RAM)");
}

void RiscvCpu::step() {
    uint32_t inst = mem_.read_word(pc_);
    execute(inst);
    if (pc_ % 4 != 0) {
        ESP_LOGW(TAG, "PC misaligned: 0x%08X", pc_);
    }
}

void RiscvCpu::run(uint32_t instructions) {
    for (uint32_t i = 0; i < instructions; ++i) step();
}

void RiscvCpu::load_program(uint32_t addr, const uint32_t *data, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        mem_.write_word(addr + i * 4, data[i]);
    }
}

uint32_t RiscvCpu::get_pc() const { return pc_; }

uint32_t RiscvCpu::get_reg(uint8_t idx) const {
    if (idx < 32) return regs_[idx];
    return 0;
}

static int32_t sign_extend(uint32_t val, int bits) {
    uint32_t mask = 1U << (bits - 1);
    return (int32_t)((val ^ mask) - mask);
}

void RiscvCpu::execute(uint32_t instruction) {
    if (instruction == 0x00000013) { // NOP (addi x0, x0, 0)
        pc_ += 4;
        return;
    }

    uint8_t opcode = instruction & 0x7F;
    uint8_t rd = (instruction >> 7) & 0x1F;
    uint8_t funct3 = (instruction >> 12) & 0x7;
    uint8_t rs1 = (instruction >> 15) & 0x1F;
    uint8_t rs2 = (instruction >> 20) & 0x1F;
    uint8_t funct7 = (instruction >> 25) & 0x7F;

    switch (opcode) {
        case 0x33: { // R-type
            int32_t a = (int32_t)regs_[rs1];
            int32_t b = (int32_t)regs_[rs2];
            uint32_t ua = regs_[rs1];
            uint32_t ub = regs_[rs2];
            switch (funct3) {
                case 0x0: regs_[rd] = (funct7 == 0x20) ? (uint32_t)(a - b) : (uint32_t)(a + b); break;
                case 0x1: regs_[rd] = ua << (ub & 0x1F); break;
                case 0x2: regs_[rd] = (a < b) ? 1 : 0; break;
                case 0x3: regs_[rd] = (ua < ub) ? 1 : 0; break;
                case 0x4: regs_[rd] = ua ^ ub; break;
                case 0x5: regs_[rd] = (funct7 == 0x20) ? (uint32_t)(a >> (ub & 0x1F)) : ua >> (ub & 0x1F); break;
                case 0x6: regs_[rd] = ua | ub; break;
                case 0x7: regs_[rd] = ua & ub; break;
            }
            if (rd == 0) regs_[0] = 0;
            pc_ += 4;
            break;
        }
        case 0x13: { // I-type (ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
            int32_t a = (int32_t)regs_[rs1];
            uint32_t ua = regs_[rs1];
            int32_t imm = sign_extend((instruction >> 20) & 0xFFF, 12);
            uint32_t uimm = (uint32_t)imm;
            switch (funct3) {
                case 0x0: regs_[rd] = (uint32_t)(a + imm); break;
                case 0x1: regs_[rd] = ua << (uimm & 0x1F); break;
                case 0x2: regs_[rd] = (a < imm) ? 1 : 0; break;
                case 0x3: regs_[rd] = (ua < uimm) ? 1 : 0; break;
                case 0x4: regs_[rd] = ua ^ uimm; break;
                case 0x5: regs_[rd] = (funct7 == 0x20) ? (uint32_t)(a >> (uimm & 0x1F)) : ua >> (uimm & 0x1F); break;
                case 0x6: regs_[rd] = ua | uimm; break;
                case 0x7: regs_[rd] = ua & uimm; break;
            }
            if (rd == 0) regs_[0] = 0;
            pc_ += 4;
            break;
        }
        case 0x03: { // Loads (LB, LH, LW, LBU, LHU)
            int32_t a = (int32_t)regs_[rs1];
            int32_t offset = sign_extend((instruction >> 20) & 0xFFF, 12);
            uint32_t addr = (uint32_t)(a + offset);
            switch (funct3) {
                case 0x0: regs_[rd] = sign_extend(mem_.read_byte(addr), 8); break;
                case 0x1: regs_[rd] = sign_extend((uint32_t)(mem_.read_byte(addr) | (mem_.read_byte(addr + 1) << 8)), 16); break;
                case 0x2: regs_[rd] = mem_.read_word(addr); break;
                case 0x4: regs_[rd] = mem_.read_byte(addr); break;
                case 0x5: regs_[rd] = (uint32_t)(mem_.read_byte(addr) | (mem_.read_byte(addr + 1) << 8)); break;
            }
            if (rd == 0) regs_[0] = 0;
            pc_ += 4;
            break;
        }
        case 0x23: { // Stores (SB, SH, SW)
            int32_t a = (int32_t)regs_[rs1];
            int32_t imm = sign_extend(((instruction >> 7) & 0x1F) | (((instruction >> 25) & 0x7F) << 5), 12);
            uint32_t addr = (uint32_t)(a + imm);
            uint32_t val = regs_[rs2];
            switch (funct3) {
                case 0x0: mem_.write_byte(addr, val & 0xFF); break;
                case 0x1: mem_.write_byte(addr, val & 0xFF); mem_.write_byte(addr + 1, (val >> 8) & 0xFF); break;
                case 0x2: mem_.write_word(addr, val); break;
            }
            pc_ += 4;
            break;
        }
        case 0x63: { // Branches (BEQ, BNE, BLT, BGE, BLTU, BGEU)
            int32_t a = (int32_t)regs_[rs1];
            int32_t b = (int32_t)regs_[rs2];
            uint32_t ua = regs_[rs1];
            uint32_t ub = regs_[rs2];
            int32_t imm = sign_extend(
                (((instruction >> 7) & 0x1) << 11) |
                (((instruction >> 8) & 0xF) << 1) |
                (((instruction >> 25) & 0x3F) << 5) |
                (((instruction >> 31) & 0x1) << 12), 13);
            bool take = false;
            switch (funct3) {
                case 0x0: take = (a == b); break;
                case 0x1: take = (a != b); break;
                case 0x4: take = (a < b); break;
                case 0x5: take = (a >= b); break;
                case 0x6: take = (ua < ub); break;
                case 0x7: take = (ua >= ub); break;
            }
            pc_ += take ? imm : 4;
            break;
        }
        case 0x37: { // LUI
            uint32_t imm = instruction & 0xFFFFF000;
            regs_[rd] = imm;
            if (rd == 0) regs_[0] = 0;
            pc_ += 4;
            break;
        }
        case 0x17: { // AUIPC
            uint32_t imm = instruction & 0xFFFFF000;
            regs_[rd] = pc_ + imm;
            if (rd == 0) regs_[0] = 0;
            pc_ += 4;
            break;
        }
        case 0x6F: { // JAL
            int32_t imm = sign_extend(
                (((instruction >> 21) & 0x3FF) << 1) |
                (((instruction >> 20) & 0x1) << 11) |
                (((instruction >> 12) & 0xFF) << 12) |
                (((instruction >> 31) & 0x1) << 20), 21);
            regs_[rd] = pc_ + 4;
            if (rd == 0) regs_[0] = 0;
            pc_ += imm;
            break;
        }
        case 0x67: { // JALR
            int32_t a = (int32_t)regs_[rs1];
            int32_t imm = sign_extend((instruction >> 20) & 0xFFF, 12);
            uint32_t target = (uint32_t)(a + imm) & ~1U;
            regs_[rd] = pc_ + 4;
            if (rd == 0) regs_[0] = 0;
            pc_ = target;
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown opcode 0x%02X at PC 0x%08X", opcode, pc_);
            pc_ += 4;
            break;
    }
}

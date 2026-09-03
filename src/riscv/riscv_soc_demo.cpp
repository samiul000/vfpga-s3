#include "riscv_soc_demo.h"
#include "riscv_cpu.h"
#include "vfpga/vfpga_core.h"
#include "hdl/mapper.h"
#include "esp_log.h"

static const char *TAG = "soc_demo";

// MMIO map (base 0x10000000):
//   +0x00  fabric input A (write)
//   +0x04  fabric input B (write)
//   +0x08  fabric output: evaluates LUT(A,B) and returns bit0 (read)
//   +0x0C  LUT truth table low 16 bits: reconfigures fabric on the fly (write)
static constexpr uint32_t MMIO_BASE = 0x10000000;
static constexpr uint32_t REG_A = 0x00;
static constexpr uint32_t REG_B = 0x04;
static constexpr uint32_t REG_Y = 0x08;
static constexpr uint32_t REG_TT = 0x0C;

static constexpr uint16_t NET_A = 10;
static constexpr uint16_t NET_B = 11;
static constexpr uint16_t NET_Y = 12;

struct SocFabric {
    VFpgaCore core;
    uint32_t a = 0;
    uint32_t b = 0;
    uint16_t truth_table = 0x8000; // AND
};

static void fabric_apply(SocFabric *f) {
    f->core.write_input(NET_A, f->a ? 0xFFFFFFFF : 0);
    f->core.write_input(NET_B, f->b ? 0xFFFFFFFF : 0);
    f->core.evaluate_combinational();
}

static uint32_t soc_read(void *ctx, uint32_t addr) {
    SocFabric *f = (SocFabric *)ctx;
    if (addr == MMIO_BASE + REG_Y) {
        fabric_apply(f);
        return f->core.read_signal(NET_Y) ? 1 : 0;
    }
    return 0;
}

static void soc_write(void *ctx, uint32_t addr, uint32_t data) {
    SocFabric *f = (SocFabric *)ctx;
    if (addr == MMIO_BASE + REG_A) f->a = data & 1;
    else if (addr == MMIO_BASE + REG_B) f->b = data & 1;
    else if (addr == MMIO_BASE + REG_TT) {
        // On-the-fly LUT reconfig
        f->truth_table = (uint16_t)(data & 0xFFFF);
        MappedConfig cfg;
        cfg.total_nets = 16;
        MappedLut ml;
        ml.id = 0;
        ml.truth_table = f->truth_table;
        ml.input_net_ids = {NET_A, NET_B, 0, 0};
        ml.output_net_id = NET_Y;
        cfg.luts.push_back(ml);
        f->core.load_config(cfg);
    }
}

void run_soc_demo() {
    ESP_LOGI(TAG, "=== Demo: RV32I + VFPGA Virtual SoC (MMIO) ===");

    SocFabric fabric;
    fabric.core.init(16, 4, 0);
    soc_write(&fabric, MMIO_BASE + REG_TT, 0x8000); // AND gate

    RiscvCpu cpu;
    cpu.reset();
    cpu.set_io_handler(&fabric, soc_read, soc_write);

    // x5=MMIO base; write A=1,B=1; read Y(expect 1); write A=0; read Y(expect 0)
    uint32_t program[] = {
        0x100002B7, // lui  x5, 0x10000   (x5 = 0x10000000)
        0x00100313, // addi x6, x0, 1
        0x0062A023, // sw   x6, 0(x5)      (A = 1)
        0x0062A223, // sw   x6, 4(x5)      (B = 1)
        0x0082A383, // lw   x7, 8(x5)      (Y -> x7, expect 1)
        0x0002A023, // sw   x0, 0(x5)      (A = 0)
        0x0082A403, // lw   x8, 8(x5)      (Y -> x8, expect 0)
        0x00000013, // nop
    };
    cpu.load_program(0, program, sizeof(program) / sizeof(program[0]));
    cpu.run(32);

    uint32_t y1 = cpu.get_reg(7);
    uint32_t y0 = cpu.get_reg(8);
    bool pass = (y1 == 1 && y0 == 0);
    ESP_LOGI(TAG, "RISC-V drove fabric via MMIO: AND(1,1)=%d (exp 1), AND(0,1)=%d (exp 0) %s",
             y1, y0, pass ? "[PASS]" : "[FAIL]");

    // C++ side: on-the-fly reconfig to OR, RISC-V visible via same MMIO map
    soc_write(&fabric, MMIO_BASE + REG_TT, 0xFFFE); // OR
    soc_write(&fabric, MMIO_BASE + REG_A, 0);
    soc_write(&fabric, MMIO_BASE + REG_B, 1);
    uint32_t y_or = soc_read(&fabric, MMIO_BASE + REG_Y);
    ESP_LOGI(TAG, "Fabric reconfigured to OR on the fly: OR(0,1)=%d (exp 1) %s",
             y_or, y_or == 1 ? "[PASS]" : "[FAIL]");
}

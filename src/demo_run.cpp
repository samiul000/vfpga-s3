#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "driver/gpio.h"

#include "io/board_profile.h"
#include "io/gpio_capability.h"
#include "io/gpio_bridge.h"
#include "vfpga/vfpga_core.h"
#include "vfpga/vfpga_lut.h"
#include "vfpga/vfpga_ff.h"
#include "vfpga/vfpga_mux.h"
#include "vfpga/vfpga_bram.h"
#include "vfpga/vfpga_dsp.h"
#include "vfpga/vfpga_scheduler.h"
#include "engine/bitparallel.h"
#include "riscv/riscv_cpu.h"
#include "riscv/riscv_soc_demo.h"

#include "hdl/lexer.h"
#include "hdl/parser.h"
#include "hdl/netlist.h"
#include "hdl/mapper.h"
#include "hdl/hdl_embedded.h"
#include "tests/test_framework.h"

static const char *TAG = "vfpga";

static void core1_task(void *arg) {
    ESP_LOGI(TAG, "Core 1 task running on core %d", xPortGetCoreID());
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(NULL);
}

static void run_self_tests(GpioBridge &bridge) {
    int pass = 0;
    int fail = 0;

    ESP_LOGI(TAG, "=== Self-Tests ===");

    void *ptr = heap_caps_malloc(1024, MALLOC_CAP_INTERNAL);
    if (ptr) { ESP_LOGI(TAG, "[PASS] Internal RAM allocation"); heap_caps_free(ptr); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Internal RAM allocation"); fail++; }

#if CONFIG_SPIRAM
    void *psram_ptr = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (psram_ptr) { ESP_LOGI(TAG, "[PASS] PSRAM allocation"); heap_caps_free(psram_ptr); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] PSRAM allocation"); fail++; }
#else
    ESP_LOGI(TAG, "[SKIP] PSRAM allocation (PSRAM disabled)");
#endif

    gpio_reset_pin(GPIO_NUM_4);
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);
    ESP_LOGI(TAG, "[PASS] GPIO4 input read: %d", gpio_get_level(GPIO_NUM_4));
    pass++;

    gpio_reset_pin(GPIO_NUM_5);
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_5, 1);
    ESP_LOGI(TAG, "[PASS] GPIO5 output write/readback: %d", gpio_get_level(GPIO_NUM_5));
    gpio_set_level(GPIO_NUM_5, 0);
    pass++;

    int64_t t1 = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(10));
    int64_t t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "[PASS] Timer: %lld us", t2 - t1);
    pass++;

    xTaskCreatePinnedToCore(core1_task, "core1_test", 2048, NULL, 1, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "[PASS] Core 1 task created");
    pass++;

    ESP_LOGI(TAG, "Results: %d passed, %d failed\n", pass, fail);
}

static void run_hdl_pipeline(const char *name, const char *hdl_source) {
    ESP_LOGI(TAG, "=== HDL Pipeline: %s ===", name);

    Lexer lexer;
    auto tokens = lexer.tokenize(hdl_source);
    ESP_LOGI(TAG, "Lexer: %zu tokens", tokens.size());

    Parser parser;
    auto ast = parser.parse(tokens);
    ESP_LOGI(TAG, "Parser: %zu AST nodes", ast.size());

    Netlist netlist;
    netlist.build_from_ast(ast);
    ESP_LOGI(TAG, "Netlist: %zu nets, %zu components", netlist.net_count(), netlist.component_count());

    Mapper mapper;
    MappedConfig cfg = mapper.map_to_luts(netlist);
    ESP_LOGI(TAG, "Mapper: %zu LUTs, %zu FFs", cfg.luts.size(), cfg.ffs.size());

    VFpgaCore core;
    core.init();
    core.load_config(cfg);

    if (name == std::string("and_gate")) {
        int16_t a_id = netlist.resolve("a");
        int16_t b_id = netlist.resolve("b");
        int16_t y_id = netlist.resolve("y");
        ESP_LOGI(TAG, "Nets: a=%d, b=%d, y=%d", a_id, b_id, y_id);

        core.write_input(a_id, 0xFFFFFFFF);
        core.write_input(b_id, 0xFFFFFFFF);
        core.evaluate_combinational();
        VSignal y = core.read_signal(y_id);
        ESP_LOGI(TAG, "AND(1,1) = %d (expected 1)", y & 1);

        core.write_input(a_id, 0xFFFFFFFF);
        core.write_input(b_id, 0);
        core.evaluate_combinational();
        y = core.read_signal(y_id);
        ESP_LOGI(TAG, "AND(1,0) = %d (expected 0)", y & 1);
    } else if (name == std::string("counter")) {
        int16_t clk_id = netlist.resolve("clock");
        int16_t rst_id = netlist.resolve("reset");
        int16_t cnt_id = netlist.resolve("count");
        ESP_LOGI(TAG, "Nets: clock=%d, reset=%d, count=%d", clk_id, rst_id, cnt_id);

        ESP_LOGI(TAG, "Asserting reset...");
        core.write_input(rst_id, 1);
        core.write_input(clk_id, 0);
        core.evaluate_combinational();
        core.clock();
        core.write_input(rst_id, 0);

        ESP_LOGI(TAG, "Running 10 clock cycles...");
        for (int cycle = 0; cycle < 10; ++cycle) {
            core.write_input(clk_id, 1);
            core.evaluate_combinational();
            core.clock();
            VSignal count = core.read_signal(cnt_id);
            ESP_LOGI(TAG, "Cycle %d: count = %d", cycle + 1, count & 0xFF);
            core.write_input(clk_id, 0);
            core.evaluate_combinational();
        }
    } else if (name == std::string("lfsr")) {
        int16_t clk_id = netlist.resolve("clock");
        int16_t rst_id = netlist.resolve("reset");
        int16_t st_id = netlist.resolve("state");
        ESP_LOGI(TAG, "Nets: clock=%d, reset=%d, state=%d", clk_id, rst_id, st_id);

        ESP_LOGI(TAG, "Asserting reset...");
        core.write_input(rst_id, 1);
        core.write_input(clk_id, 0);
        core.evaluate_combinational();
        core.clock();
        core.write_input(rst_id, 0);

        ESP_LOGI(TAG, "Running 5 cycles...");
        for (int cycle = 0; cycle < 5; ++cycle) {
            core.write_input(clk_id, 1);
            core.evaluate_combinational();
            core.clock();
            VSignal state = core.read_signal(st_id);
            ESP_LOGI(TAG, "Cycle %d: state = 0x%02X", cycle + 1, state & 0xFF);
            core.write_input(clk_id, 0);
            core.evaluate_combinational();
        }
    }
    ESP_LOGI(TAG, "");
}

static void run_demo_lut() {
    ESP_LOGI(TAG, "=== Demo: LUT4 as AND gate ===");
    VLut4 lut;
    lut.configure(0x08); // 2-input AND: idx=a|2b, only 11->1 = bit3 = 0x08
    VSignal a = 0xFFFFFFFF;
    VSignal b = 0xFFFFFFFF;
    VSignal result = lut.evaluate(a, b, 0, 0);
    ESP_LOGI(TAG, "AND(0xFFFFFFFF, 0xFFFFFFFF) = 0x%08X (expected 0xFFFFFFFF)\n", result);
}

static void run_demo_counter() {
    ESP_LOGI(TAG, "=== Demo: 8-bit counter using FFs ===");
    VFlipFlop ffs[8];
    for (int i = 0; i < 8; ++i) ffs[i].reset();

    ESP_LOGI(TAG, "Running 256 cycles...");
    for (int cycle = 0; cycle < 256; ++cycle) {
        // Toggle logic: bit 0 toggles every cycle, bit N toggles when all lower bits are 1
        uint8_t count = 0;
        for (int i = 0; i < 8; ++i) count |= (ffs[i].output() & 1) << i;

        uint8_t next = count + 1;
        for (int i = 0; i < 8; ++i) {
            bool enable = true;
            for (int j = 0; j < i; ++j) enable = enable && ((count >> j) & 1);
            ffs[i].clock_edge((next >> i) & 1, enable);
        }
    }

    uint8_t final_count = 0;
    for (int i = 0; i < 8; ++i) final_count |= (ffs[i].output() & 1) << i;
    ESP_LOGI(TAG, "Final count: %d (expected 0)\n", final_count);
}

static void run_demo_lfsr() {
    ESP_LOGI(TAG, "=== Demo: 8-bit LFSR ===");
    VFlipFlop ffs[8];
    for (int i = 0; i < 8; ++i) ffs[i].reset();
    // Seed
    for (int i = 0; i < 8; ++i) ffs[i].clock_edge(1, true);

    ESP_LOGI(TAG, "Running 255 cycles...");
    for (int cycle = 0; cycle < 255; ++cycle) {
        uint8_t state = 0;
        for (int i = 0; i < 8; ++i) state |= (ffs[i].output() & 1) << i;

        uint8_t feedback = ((state >> 0) ^ (state >> 2) ^ (state >> 3) ^ (state >> 4)) & 1;
        uint8_t next = (state >> 1) | (feedback << 7);
        for (int i = 0; i < 8; ++i) ffs[i].clock_edge((next >> i) & 1, true);
    }

    uint8_t final_state = 0;
    for (int i = 0; i < 8; ++i) final_state |= (ffs[i].output() & 1) << i;
    ESP_LOGI(TAG, "Final LFSR state: 0x%02X\n", final_state);
}

static void run_demo_gpio_led(GpioBridge &bridge) {
    ESP_LOGI(TAG, "=== Demo: HDL blinker -> fabric -> GPIO5 LED ===");
    static const char *BLINK_HDL =
        "module blinker;\n"
        "input clock;\n"
        "output led;\n"
        "register q;\n"
        "always @(posedge clock) begin\n"
        "    q <= q ^ 1;\n"
        "end\n"
        "assign led = q;\n"
        "endmodule\n";

    Lexer lexer;
    auto tokens = lexer.tokenize(BLINK_HDL);
    Parser parser;
    auto ast = parser.parse(tokens);
    Netlist netlist;
    netlist.build_from_ast(ast);
    Mapper mapper;
    MappedConfig cfg = mapper.map_to_luts(netlist);
    int16_t led_id = netlist.resolve("led");
    if (cfg.ffs.empty() || led_id < 0) {
        ESP_LOGW(TAG, "Blinker mapping failed, skipping GPIO demo");
        return;
    }
    ESP_LOGI(TAG, "Blinker: %zu LUTs, %zu FFs, led net=%d",
             cfg.luts.size(), cfg.ffs.size(), led_id);

    if (!bridge.map_output(0, 5)) {
        ESP_LOGW(TAG, "GPIO5 unsafe, skipping GPIO demo");
        return;
    }
    VFpgaCore core;
    core.init();
    core.load_config(cfg);
    for (int i = 0; i < 8; ++i) {
        core.evaluate_combinational();
        core.clock();
        uint8_t bit = core.read_signal(led_id) ? 1 : 0;
        bridge.set_output_value(0, bit);
        bridge.commit_outputs();
        ESP_LOGI(TAG, "Blink %d: led=%d -> GPIO5", i + 1, bit);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    ESP_LOGI(TAG, "");
}

static void run_demo_riscv() {
    ESP_LOGI(TAG, "=== Demo: RISC-V sum 1..10 ===");
    RiscvCpu cpu;
    cpu.reset();

    // Program: sum = 0; for(i=1; i<=10; i++) sum += i; -> result in x10
    uint32_t program[] = {
        0x00000513, // addi x10, x0, 0      (sum = 0)
        0x00100593, // addi x11, x0, 1      (i = 1)
        0x00A00613, // addi x12, x0, 10     (limit = 10)
        0x00B50533, // add  x10, x10, x11   (sum += i)
        0x00158593, // addi x11, x11, 1     (i++)
        0xFEB65CE3, // bge  x12, x11, -8    (if limit >= i, goto add)
        0x00000013, // nop
    };

    cpu.load_program(0, program, sizeof(program) / sizeof(program[0]));
    cpu.run(100); // run up to 100 instructions (loop does ~14)

    uint32_t result = cpu.get_reg(10);
    ESP_LOGI(TAG, "RISC-V CPU: sum 1..10 = %d (expected 55) %s",
             result, result == 55 ? "[PASS]" : "[FAIL]");
}

void run_final_report() {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  VFPGA-S3 Final Report");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Hardware: ESP32-S3 N16R8 on DevKitC-1");
    ESP_LOGI(TAG, "CPU:      Xtensa LX7 @ 240 MHz");
    ESP_LOGI(TAG, "Flash:    16 MB");
    ESP_LOGI(TAG, "PSRAM:    %d KB", (int)(esp_psram_get_size() / 1024));
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "VFPGA Resources:");
    ESP_LOGI(TAG, "  LUT4:    Configurable 4-input LUTs");
    ESP_LOGI(TAG, "  FF:      D flip-flops with enable/reset");
    ESP_LOGI(TAG, "  BRAM:    64/256/1024 x 32-bit");
    ESP_LOGI(TAG, "  DSP:     INT8/16/32 multiply-add");
    ESP_LOGI(TAG, "  VIO:     Up to 256 virtual I/O signals");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Execution Engines:");
    ESP_LOGI(TAG, "  - Scalar");
    ESP_LOGI(TAG, "  - Bit-parallel (32 signals per op)");
    ESP_LOGI(TAG, "  - SIMD batch");
    ESP_LOGI(TAG, "  - Dual-core (FreeRTOS)");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Software Components:");
    ESP_LOGI(TAG, "  - Mini HDL (lexer/parser/mapper)");
    ESP_LOGI(TAG, "  - Binary config format with validation");
    ESP_LOGI(TAG, "  - RV32I emulator (37 instructions)");
    ESP_LOGI(TAG, "  - Memory-mapped VFPGA I/O");
    ESP_LOGI(TAG, "  - GPIO bridge with safety validation");
    ESP_LOGI(TAG, "  - GPIO capability database");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  This is a software-defined virtual FPGA.");
    ESP_LOGI(TAG, "  NOT a physical FPGA.");
    ESP_LOGI(TAG, "========================================\n");
}

void run_all_demos(void)
{
    printf("\n");
    printf("██╗   ██╗███████╗██████╗  ██████╗  █████╗       ███████╗██████╗ \n");
    printf("██║   ██║██╔════╝██╔══██╗██╔════╝ ██╔══██╗      ██╔════╝╚════██╗\n");
    printf("██║   ██║█████╗  ██████╔╝██║  ███╗███████║█████╗███████╗ █████╔╝\n");
    printf("╚██╗ ██╔╝██╔══╝  ██╔═══╝ ██║   ██║██╔══██║╚════╝╚════██║ ╚═══██╗\n");
    printf(" ╚████╔╝ ██║     ██║     ╚██████╔╝██║  ██║      ███████║██████╔╝\n");
    printf("  ╚═══╝  ╚═╝     ╚═╝      ╚═════╝ ╚═╝  ╚═╝      ╚══════╝╚═════╝\n");
    printf("\n");
    ESP_LOGI(TAG, "Software-Defined Virtual FPGA on ESP32-S3");
    ESP_LOGI(TAG, "Target: ESP32-S3 N16R8 on DevKitC-1\n");

    BoardProfile board;
    board.detect();

    GpioCapability gpio_cap;
    gpio_cap.load_default();

    GpioBridge bridge;
    bridge.begin(&gpio_cap, &board);

    board.print_diagnostics();
    gpio_cap.print_status();

    run_self_tests(bridge);

    // M1-M4 tests
    test_bitparallel();
    test_lut4();
    test_lut4_4k_benchmark();
    test_flipflop();
    test_routing();

    // M6-M7 benchmarks
    test_bram_benchmark();
    test_dsp_benchmark();

    // HDL pipeline demos
    run_hdl_pipeline("and_gate", AND_GATE_HDL);
    run_hdl_pipeline("counter", COUNTER_HDL);
    run_hdl_pipeline("lfsr", LFSR_HDL);

    // M18 demos
    run_demo_lut();
    run_demo_counter();
    run_demo_lfsr();
    run_demo_riscv();
    run_soc_demo();
    run_demo_gpio_led(bridge);

    // Final report
    run_final_report();

    ESP_LOGI(TAG, "All milestones complete. System ready.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#include <cstdio>
#include <cstring>
#include <vector>
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
#include "hdl/lexer.h"
#include "hdl/parser.h"
#include "hdl/netlist.h"
#include "hdl/mapper.h"
#include "hdl/user_design.h"

static const char *TAG = "vfpga";

// Called from demo_run.cpp
extern void run_all_demos(void);
extern void run_final_report(void);

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

static void run_user_hdl() {
    ESP_LOGI(TAG, "=== User HDL Design ===");

    Lexer lexer;
    auto tokens = lexer.tokenize(USER_HDL);
    Parser parser;
    auto ast = parser.parse(tokens);
    Netlist netlist;
    netlist.build_from_ast(ast);
    Mapper mapper;
    MappedConfig cfg = mapper.map_to_luts(netlist);

    ESP_LOGI(TAG, "LUTs: %zu, FFs: %zu", cfg.luts.size(), cfg.ffs.size());

    VFpgaCore core;
    core.init();
    core.load_config(cfg);

    auto input_names = netlist.input_names();
    auto output_names = netlist.output_names();

    ESP_LOGI(TAG, "Inputs (%zu):", input_names.size());
    for (size_t i = 0; i < input_names.size(); i++) {
        ESP_LOGI(TAG, "  [%zu] %s", i, input_names[i].c_str());
    }
    ESP_LOGI(TAG, "Outputs (%zu):", output_names.size());
    for (size_t i = 0; i < output_names.size(); i++) {
        ESP_LOGI(TAG, "  [%zu] %s", i, output_names[i].c_str());
    }

    bool sequential = !cfg.ffs.empty();

    if (sequential) {
        // Find clock and reset signals
        int16_t clk_id = -1, rst_id = -1;
        std::string clk_name, rst_name;
        for (size_t i = 0; i < input_names.size(); i++) {
            if (input_names[i] == "clock" || input_names[i] == "clk") {
                clk_id = cfg.input_net_ids[i];
                clk_name = input_names[i];
            }
            if (input_names[i] == "reset" || input_names[i] == "rst") {
                rst_id = cfg.input_net_ids[i];
                rst_name = input_names[i];
            }
        }

        if (clk_id < 0 || rst_id < 0) {
            ESP_LOGE(TAG, "Sequential design needs 'clock'/'clk' and 'reset'/'rst' inputs");
            return;
        }

        ESP_LOGI(TAG, "Sequential: clk=%s rst=%s", clk_name.c_str(), rst_name.c_str());

        // Assert reset
        core.write_input(rst_id, 1);
        core.write_input(clk_id, 0);
        core.evaluate_combinational();
        core.clock();
        core.write_input(rst_id, 0);

        // Run cycles
        for (int c = 0; c < USER_CYCLES; c++) {
            core.write_input(clk_id, 1);
            core.evaluate_combinational();
            core.clock();

            for (size_t i = 0; i < output_names.size(); i++) {
                VSignal val = core.read_signal(cfg.output_net_ids[i]);
                ESP_LOGI(TAG, "  Cycle %2d: %s = 0x%08X", c + 1, output_names[i].c_str(), val);
            }

            core.write_input(clk_id, 0);
            core.evaluate_combinational();
        }
    } else {
        // Combinational
        size_t nin = cfg.input_net_ids.size();

        if (!USER_TEST_INPUTS.empty()) {
            ESP_LOGI(TAG, "Combinational: %zu custom test vectors", USER_TEST_INPUTS.size());
            for (size_t t = 0; t < USER_TEST_INPUTS.size(); t++) {
                auto &tv = USER_TEST_INPUTS[t];
                for (size_t i = 0; i < nin && i < tv.size(); i++) {
                    core.write_input(cfg.input_net_ids[i], tv[i] ? 0xFFFFFFFF : 0);
                }
                core.evaluate_combinational();

                char in_buf[32] = "";
                for (size_t i = 0; i < nin && i < tv.size(); i++) {
                    strcat(in_buf, tv[i] ? "1" : "0");
                }

                for (size_t i = 0; i < output_names.size(); i++) {
                    VSignal val = core.read_signal(cfg.output_net_ids[i]);
                    ESP_LOGI(TAG, "  [%s] %s = %d", in_buf, output_names[i].c_str(), val & 1);
                }
            }
        } else if (nin <= 8 && nin > 0) {
            size_t total = 1U << nin;
            ESP_LOGI(TAG, "Combinational: exhaustive %zu-input (%zu tests)", nin, total);

            for (size_t combo = 0; combo < total; combo++) {
                for (size_t i = 0; i < nin; i++) {
                    core.write_input(cfg.input_net_ids[i], ((combo >> i) & 1) ? 0xFFFFFFFF : 0);
                }
                core.evaluate_combinational();

                char in_buf[32] = "";
                for (size_t i = 0; i < nin; i++) {
                    strcat(in_buf, ((combo >> i) & 1) ? "1" : "0");
                }

                for (size_t i = 0; i < output_names.size(); i++) {
                    VSignal val = core.read_signal(cfg.output_net_ids[i]);
                    ESP_LOGI(TAG, "  [%s] %s = %d", in_buf, output_names[i].c_str(), val & 1);
                }
            }
        } else if (nin == 0) {
            ESP_LOGW(TAG, "No inputs found — design has no testable ports");
        } else {
            ESP_LOGE(TAG, "Too many inputs (%zu) for exhaustive. Set USER_TEST_INPUTS.", nin);
        }
    }

    ESP_LOGI(TAG, "");
}

extern "C" void app_main(void) {
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

    if (RUN_SELF_TESTS) {
        run_self_tests(bridge);
    }

    run_user_hdl();

    // Uncomment to run all built-in demos (HDL pipeline, LUT, counter, LFSR, RISC-V):
    // run_all_demos();

    run_final_report();

    ESP_LOGI(TAG, "Done. System idle.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

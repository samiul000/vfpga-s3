#include "test_framework.h"
#include "vfpga/vfpga_ff.h"
#include "esp_log.h"

static const char *TAG = "test_ff";

void test_flipflop() {
    ESP_LOGI(TAG, "=== M3: Flip-Flop Tests ===");
    int pass = 0;
    int fail = 0;

    VFlipFlop ff;

    // Test 1: Reset
    ff.reset();
    if (ff.output() == 0) { ESP_LOGI(TAG, "[PASS] Reset clears Q to 0"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Reset: Q=0x%08X", ff.output()); fail++; }

    // Test 2: Rising edge latches D
    ff.reset();
    ff.clock_edge(0xDEADBEEF, true);
    if (ff.output() == 0xDEADBEEF) { ESP_LOGI(TAG, "[PASS] Rising edge latches D"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Rising edge: Q=0x%08X expected 0xDEADBEEF", ff.output()); fail++; }

    // Test 3: Enable=false retains state
    ff.reset();
    ff.clock_edge(0x12345678, true);
    ff.clock_edge(0xAAAAAAAA, false);
    if (ff.output() == 0x12345678) { ESP_LOGI(TAG, "[PASS] Enable=false retains state"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Enable=false: Q=0x%08X expected 0x12345678", ff.output()); fail++; }

    // Test 4: State retention across multiple cycles
    ff.reset();
    ff.clock_edge(0x11111111, true);
    ff.clock_edge(0x22222222, true);
    ff.clock_edge(0x33333333, true);
    if (ff.output() == 0x33333333) { ESP_LOGI(TAG, "[PASS] State retention across cycles"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] State retention: Q=0x%08X", ff.output()); fail++; }

    // Test 5: Reset after data
    ff.clock_edge(0xABCDABCD, true);
    ff.reset();
    if (ff.output() == 0) { ESP_LOGI(TAG, "[PASS] Reset after data clears Q"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Reset after data: Q=0x%08X", ff.output()); fail++; }

    // Test 6: Enable without clock edge
    ff.reset();
    ff.clock_edge(0x12345678, true);
    ff.clock_edge(0x99999999, false);
    if (ff.output() == 0x12345678) { ESP_LOGI(TAG, "[PASS] Enable without edge retains"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Enable without edge: Q=0x%08X", ff.output()); fail++; }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Flip-Flop: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}

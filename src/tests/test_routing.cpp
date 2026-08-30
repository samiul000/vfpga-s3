#include "test_framework.h"
#include "vfpga/vfpga_scheduler.h"
#include "esp_log.h"

static const char *TAG = "test_route";

void test_routing() {
    ESP_LOGI(TAG, "=== M4: Routing Tests ===");
    int pass = 0;
    int fail = 0;

    VFpgaScheduler sched;
    sched.init(16);

    // Test 1: Set and get signal
    sched.set_signal(0, 0xDEADBEEF);
    VSignal val = sched.get_signal(0);
    if (val == 0xDEADBEEF) { ESP_LOGI(TAG, "[PASS] Set/get signal"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Set/get: 0x%08X", val); fail++; }

    // Test 2: Route A -> B
    sched.set_signal(0, 0xAAAAAAAA);
    sched.route(0, 1);
    sched.evaluate();
    val = sched.get_signal(1);
    if (val == 0xAAAAAAAA) { ESP_LOGI(TAG, "[PASS] Route A->B"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Route A->B: 0x%08X", val); fail++; }

    // Test 3: Fan-out A -> {B, C, D}
    sched.init(16);
    sched.set_signal(0, 0x12345678);
    sched.route(0, 1);
    sched.route(0, 2);
    sched.route(0, 3);
    sched.evaluate();
    VSignal b = sched.get_signal(1);
    VSignal c = sched.get_signal(2);
    VSignal d = sched.get_signal(3);
    if (b == 0x12345678 && c == 0x12345678 && d == 0x12345678) {
        ESP_LOGI(TAG, "[PASS] Fan-out A->{B,C,D}");
        pass++;
    } else {
        ESP_LOGE(TAG, "[FAIL] Fan-out: B=0x%08X C=0x%08X D=0x%08X", b, c, d);
        fail++;
    }

    // Test 4: Chain A -> B -> C
    sched.init(16);
    sched.set_signal(0, 0xABCDABCD);
    sched.route(0, 1);
    sched.route(1, 2);
    sched.evaluate();
    val = sched.get_signal(2);
    if (val == 0xABCDABCD) { ESP_LOGI(TAG, "[PASS] Chain A->B->C"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Chain: 0x%08X", val); fail++; }

    // Test 5: Overwrite signal
    sched.init(16);
    sched.set_signal(0, 0x11111111);
    sched.route(0, 1);
    sched.evaluate();
    sched.set_signal(0, 0x22222222);
    sched.evaluate();
    val = sched.get_signal(1);
    if (val == 0x22222222) { ESP_LOGI(TAG, "[PASS] Overwrite signal propagates"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] Overwrite: 0x%08X", val); fail++; }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Routing: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}

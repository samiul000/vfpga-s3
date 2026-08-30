#include "test_framework.h"
#include "vfpga/vfpga_core.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "test_dualcore";

static void vfpga_worker_task(void *arg) {
    VFpgaCore *core = (VFpgaCore *)arg;
    core->run_cycles(50000);
    vTaskDelete(NULL);
}

void test_dualcore_benchmark() {
    ESP_LOGI(TAG, "=== M11: Dual-Core Benchmark ===");

    const uint32_t CYCLES = 100000;
    int64_t t1, t2;

    // Single-core
    VFpgaCore core_sc;
    core_sc.initialize();
    t1 = esp_timer_get_time();
    core_sc.run_cycles(CYCLES);
    t2 = esp_timer_get_time();
    double sc_us = t2 - t1;
    ESP_LOGI(TAG, "Single-core: %.0f us for %u cycles (%.1f Kcycles/s)", sc_us, CYCLES, CYCLES / sc_us * 1000);

    // Dual-core
    VFpgaCore core_dc;
    core_dc.initialize();
    t1 = esp_timer_get_time();
    xTaskCreatePinnedToCore(vfpga_worker_task, "vfpga_dc", 4096, &core_dc, 1, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    t2 = esp_timer_get_time();
    double dc_us = t2 - t1;
    ESP_LOGI(TAG, "Dual-core:  %.0f us (control on core 0, VFPGA on core 1)", dc_us);

    double speedup = sc_us / dc_us;
    ESP_LOGI(TAG, "Speedup:    %.2fx", speedup);
    ESP_LOGI(TAG, "");
}

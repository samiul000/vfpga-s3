#include "benchmark.h"
#include "esp_log.h"

static const char *TAG = "lfsr_bench";

void Benchmark::run_lfsr() {
    ESP_LOGI(TAG, "=== LFSR Benchmark ===");
    uint32_t lfsr = 0xDEADBEEF;
    for (int i = 0; i < 1000000; ++i) {
        uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 1) ^ (lfsr >> 21) ^ (lfsr >> 31)) & 1;
        lfsr = (lfsr >> 1) | (bit << 31);
    }
    ESP_LOGI(TAG, "LFSR after 1M iterations: 0x%08X", lfsr);
}

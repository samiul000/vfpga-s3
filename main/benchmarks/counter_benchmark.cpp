#include "benchmark.h"
#include "esp_log.h"

static const char *TAG = "counter_bench";

void Benchmark::run_counter() {
    ESP_LOGI(TAG, "=== Counter Benchmark ===");
    uint32_t counter = 0;
    for (int i = 0; i < 1000000; ++i) counter++;
    ESP_LOGI(TAG, "Counter after 1M increments: %u", counter);
}

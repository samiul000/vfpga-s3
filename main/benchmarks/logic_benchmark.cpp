#include "benchmark.h"
#include "engine/bitparallel.h"
#include "esp_log.h"

static const char *TAG = "logic_bench";

void Benchmark::run_logic() {
    ESP_LOGI(TAG, "=== Logic Benchmark ===");
    VSignal a = 0xAAAAAAAA;
    VSignal b = 0x55555555;
    VSignal result = BitParallel::op_and(a, b);
    ESP_LOGI(TAG, "AND result: 0x%08X (expected 0x00000000)", result);
    result = BitParallel::op_or(a, b);
    ESP_LOGI(TAG, "OR  result: 0x%08X (expected 0xFFFFFFFF)", result);
    result = BitParallel::op_xor(a, b);
    ESP_LOGI(TAG, "XOR result: 0x%08X (expected 0xFFFFFFFF)", result);
}

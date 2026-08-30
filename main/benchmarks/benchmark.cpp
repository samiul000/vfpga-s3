#include "benchmark.h"
#include "esp_log.h"

static const char *TAG = "benchmark";

void Benchmark::run_all() {
    ESP_LOGI(TAG, "=== Running Benchmark Suite ===");
    run_logic();
    run_counter();
    run_lfsr();
    run_nn();
    ESP_LOGI(TAG, "=== Benchmark Suite Complete ===");
}

void Benchmark::run_logic() { ESP_LOGI(TAG, "Logic benchmark placeholder"); }
void Benchmark::run_counter() { ESP_LOGI(TAG, "Counter benchmark placeholder"); }
void Benchmark::run_lfsr() { ESP_LOGI(TAG, "LFSR benchmark placeholder"); }
void Benchmark::run_nn() { ESP_LOGI(TAG, "NN benchmark placeholder"); }

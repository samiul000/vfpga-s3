#include "benchmark.h"
#include "bitparallel.h"
#include "vfpga/vfpga_lut.h"
#include "vfpga/vfpga_ff.h"
#include "vfpga/vfpga_bram.h"
#include "vfpga/vfpga_dsp.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "benchmark";

static int64_t time_us() { return esp_timer_get_time(); }

void Benchmark::run_all() {
    ESP_LOGI(TAG, "=== VFPGA Benchmark Suite ===\n");
    run_logic();
    run_counter();
    run_lfsr();
    run_nn();
    ESP_LOGI(TAG, "=== Benchmark Suite Complete ===\n");
}

void Benchmark::run_logic() {
    ESP_LOGI(TAG, "--- Logic Benchmark ---");
    const int N = 100000;
    VSignal a = 0xAAAAAAAA, b = 0x55555555;

    int64_t t = time_us();
    volatile VSignal r = 0;
    for (int i = 0; i < N; ++i) r = BitParallel::op_and(a, b);
    double and_us = time_us() - t;

    t = time_us();
    for (int i = 0; i < N; ++i) r = BitParallel::op_or(a, b);
    double or_us = time_us() - t;

    t = time_us();
    for (int i = 0; i < N; ++i) r = BitParallel::op_xor(a, b);
    double xor_us = time_us() - t;

    ESP_LOGI(TAG, "AND: %.1f us for %d ops (%.1f Mops/s)", and_us, N, N / and_us);
    ESP_LOGI(TAG, "OR:  %.1f us for %d ops (%.1f Mops/s)", or_us, N, N / or_us);
    ESP_LOGI(TAG, "XOR: %.1f us for %d ops (%.1f Mops/s)\n", xor_us, N, N / xor_us);
}

void Benchmark::run_counter() {
    ESP_LOGI(TAG, "--- Counter Benchmark ---");
    const int N = 1000000;
    int64_t t = time_us();
    uint32_t counter = 0;
    for (int i = 0; i < N; ++i) counter++;
    double us = time_us() - t;
    ESP_LOGI(TAG, "32-bit counter: %d increments in %.0f us (%.1f Mops/s)\n", N, us, N / us);
}

void Benchmark::run_lfsr() {
    ESP_LOGI(TAG, "--- LFSR Benchmark ---");
    const int N = 1000000;
    int64_t t = time_us();
    uint32_t lfsr = 0xDEADBEEF;
    for (int i = 0; i < N; ++i) {
        uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 1) ^ (lfsr >> 21) ^ (lfsr >> 31)) & 1;
        lfsr = (lfsr >> 1) | (bit << 31);
    }
    double us = time_us() - t;
    ESP_LOGI(TAG, "32-bit LFSR: %d iterations in %.0f us (%.1f Mops/s)", N, us, N / us);
    ESP_LOGI(TAG, "Final state: 0x%08X\n", lfsr);
}

void Benchmark::run_nn() {
    ESP_LOGI(TAG, "--- INT8 Neural Network Benchmark ---");
    const int N = 10000;
    int8_t weights[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int8_t inputs[] = {10, 20, 30, 40, 50, 60, 70, 80};

    int64_t t = time_us();
    int32_t acc = 0;
    for (int i = 0; i < N; ++i) {
        acc = 0;
        for (int j = 0; j < 8; ++j) acc = VDsp::multiply_add(acc, weights[j], inputs[j]);
    }
    double us = time_us() - t;
    ESP_LOGI(TAG, "8-element INT8 dot product: %d iterations in %.0f us", N, us);
    ESP_LOGI(TAG, "Result: %d (expected 20400)", acc);
    ESP_LOGI(TAG, "Throughput: %.1f Kops/s\n", N / us * 1000);
}

#include "test_framework.h"
#include "vfpga/vfpga_bram.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "test_bram";

void test_bram_benchmark() {
    ESP_LOGI(TAG, "=== M6: Virtual BRAM Benchmark ===");
    int pass = 0;
    int fail = 0;

    VBram bram64;
    VBram bram256;
    VBram bram1024;

    bram64.init(VBram::Size::S64);
    bram256.init(VBram::Size::S256);
    bram1024.init(VBram::Size::S1024);

    // Test basic read/write
    bram64.write(0, 0xDEADBEEF);
    if (bram64.read(0) == 0xDEADBEEF) { ESP_LOGI(TAG, "[PASS] BRAM64 write/read"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] BRAM64 write/read"); fail++; }

    bram256.write(100, 0xCAFEBABE);
    if (bram256.read(100) == 0xCAFEBABE) { ESP_LOGI(TAG, "[PASS] BRAM256 write/read"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] BRAM256 write/read"); fail++; }

    bram1024.write(500, 0x12345678);
    if (bram1024.read(500) == 0x12345678) { ESP_LOGI(TAG, "[PASS] BRAM1024 write/read"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] BRAM1024 write/read"); fail++; }

    // Benchmark: write latency
    int64_t t1, t2;
    const int ITERATIONS = 10000;

    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) bram64.write(i % 64, i);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM64  write: %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) bram256.write(i % 256, i);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM256 write: %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) bram1024.write(i % 1024, i);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM1024 write: %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    // Benchmark: read latency
    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) volatile uint32_t v = bram64.read(i % 64);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM64  read:  %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) volatile uint32_t v = bram256.read(i % 256);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM256 read:  %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) volatile uint32_t v = bram1024.read(i % 1024);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "BRAM1024 read: %lld us for %d ops (%.1f us/op)", t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "BRAM: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}

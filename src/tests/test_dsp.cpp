#include "test_framework.h"
#include "vfpga/vfpga_dsp.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "test_dsp";

void test_dsp_benchmark() {
    ESP_LOGI(TAG, "=== M7: Virtual DSP Benchmark ===");
    int pass = 0;
    int fail = 0;

    const int ITERATIONS = 100000;
    int64_t t1, t2;

    // Test correctness
    int32_t ma = VDsp::multiply_add(3, 4, 5);
    if (ma == 17) { ESP_LOGI(TAG, "[PASS] multiply_add: 3*4+5=17"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] multiply_add: %d", ma); fail++; }

    int8_t m8 = VDsp::multiply_i8(7, 6);
    if (m8 == 42) { ESP_LOGI(TAG, "[PASS] multiply_i8: 7*6=42"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] multiply_i8: %d", m8); fail++; }

    int16_t m16 = VDsp::multiply_i16(100, 200);
    if (m16 == 20000) { ESP_LOGI(TAG, "[PASS] multiply_i16: 100*200=20000"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] multiply_i16: %d", m16); fail++; }

    int32_t m32 = VDsp::multiply_i32(1000, 1000);
    if (m32 == 1000000) { ESP_LOGI(TAG, "[PASS] multiply_i32: 1000*1000=1000000"); pass++; }
    else { ESP_LOGE(TAG, "[FAIL] multiply_i32: %d", m32); fail++; }

    // Benchmark INT8
    volatile int8_t result8 = 0;
    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) result8 = VDsp::multiply_i8(i, i + 1);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "INT8 multiply:  %lld us for %d ops (%.2f us/op, %.1f Mops/s)",
             t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS,
             (double)ITERATIONS / (t2 - t1));

    // Benchmark INT16
    volatile int16_t result16 = 0;
    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) result16 = VDsp::multiply_i16(i, i + 1);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "INT16 multiply: %lld us for %d ops (%.2f us/op, %.1f Mops/s)",
             t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS,
             (double)ITERATIONS / (t2 - t1));

    // Benchmark INT32
    volatile int32_t result32 = 0;
    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) result32 = VDsp::multiply_i32(i, i + 1);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "INT32 multiply: %lld us for %d ops (%.2f us/op, %.1f Mops/s)",
             t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS,
             (double)ITERATIONS / (t2 - t1));

    // Benchmark multiply-add
    volatile int32_t result_ma = 0;
    t1 = esp_timer_get_time();
    for (int i = 0; i < ITERATIONS; ++i) result_ma = VDsp::multiply_add(i, i + 1, i);
    t2 = esp_timer_get_time();
    ESP_LOGI(TAG, "Multiply-add:   %lld us for %d ops (%.2f us/op, %.1f Mops/s)",
             t2 - t1, ITERATIONS, (double)(t2 - t1) / ITERATIONS,
             (double)ITERATIONS / (t2 - t1));

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "DSP: %d passed, %d failed", pass, fail);
    ESP_LOGI(TAG, "");
}

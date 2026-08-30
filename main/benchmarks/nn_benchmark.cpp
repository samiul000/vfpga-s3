#include "benchmark.h"
#include "vfpga/vfpga_dsp.h"
#include "esp_log.h"

static const char *TAG = "nn_bench";

void Benchmark::run_nn() {
    ESP_LOGI(TAG, "=== INT8 Neural Network Benchmark ===");
    int8_t weights[] = {1, 2, 3, 4};
    int8_t inputs[] = {5, 6, 7, 8};
    int32_t acc = 0;
    for (int i = 0; i < 4; ++i) {
        acc = VDsp::multiply_add(acc, weights[i], inputs[i]);
    }
    ESP_LOGI(TAG, "Dot product result: %d", acc);
}

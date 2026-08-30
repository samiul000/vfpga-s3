#include "execution_engine.h"
#include "bitparallel.h"
#include "simd_engine.h"
#include "esp_log.h"

static const char *TAG = "exec_engine";

void ExecutionEngine::set_mode(Mode mode) {
    mode_ = mode;
    const char *mode_str[] = {"SCALAR", "BITPARALLEL", "SIMD", "DUAL_CORE"};
    ESP_LOGI(TAG, "Execution engine mode: %s", mode_str[static_cast<int>(mode)]);
}

ExecutionEngine::Mode ExecutionEngine::get_mode() const { return mode_; }

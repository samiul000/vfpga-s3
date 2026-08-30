#include "board_profile.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

static const char *TAG = "board";

static uint32_t s_flash_size = 0;
static size_t s_psram_size = 0;

void BoardProfile::detect() {
    esp_flash_get_size(NULL, &s_flash_size);
#if CONFIG_SPIRAM
    s_psram_size = esp_psram_get_size();
#else
    s_psram_size = 0;
#endif
    ESP_LOGI(TAG, "Board profile detected: ESP32-S3-DevKitC-1 N16R8");
}

void BoardProfile::print_diagnostics() const {
    esp_chip_info_t info;
    esp_chip_info(&info);

    ESP_LOGI(TAG, "=== VFPGA Hardware Diagnostic ===");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Board:      ESP32-S3-DevKitC-1 N16R8");
    ESP_LOGI(TAG, "Chip:       ESP32-S3 rev %d", info.revision);
    ESP_LOGI(TAG, "CPU:        Xtensa LX7");
    ESP_LOGI(TAG, "Cores:      %d", info.cores);
    ESP_LOGI(TAG, "CPU freq:   240 MHz");
    ESP_LOGI(TAG, "Flash:      %d MB", (int)(s_flash_size / (1024 * 1024)));
    ESP_LOGI(TAG, "PSRAM:      %d KB", (int)(s_psram_size / 1024));
    ESP_LOGI(TAG, "Int. RAM:   %d bytes free", (int)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free heap:  %d bytes", (int)esp_get_free_heap_size());
#if CONFIG_SPIRAM
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#else
    ESP_LOGI(TAG, "Free PSRAM: N/A (PSRAM disabled)");
#endif
    ESP_LOGI(TAG, "");
}

const char* BoardProfile::get_board_name() const { return "ESP32-S3-DevKitC-1 N16R8"; }
const char* BoardProfile::get_chip_model() const { return "ESP32-S3"; }
uint32_t BoardProfile::get_chip_revision() const {
    esp_chip_info_t info;
    esp_chip_info(&info);
    return info.revision;
}
uint32_t BoardProfile::get_cpu_freq() const { return 240; }
uint32_t BoardProfile::get_core_count() const {
    esp_chip_info_t info;
    esp_chip_info(&info);
    return info.cores;
}
size_t BoardProfile::get_flash_size() const { return s_flash_size; }
size_t BoardProfile::get_psram_size() const { return s_psram_size; }

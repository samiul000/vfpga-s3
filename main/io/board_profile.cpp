#include "board_profile.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"

static const char *TAG = "board";

void BoardProfile::detect() {
    ESP_LOGI(TAG, "Board profile detected: ESP32-S3-DevKitC-1 N16R8");
}

void BoardProfile::print_diagnostics() const {
    chip_info_t info;
    esp_chip_info(&info);
    ESP_LOGI(TAG, "=== VFPGA Hardware Diagnostic ===");
    ESP_LOGI(TAG, "Board:      ESP32-S3-DevKitC-1 N16R8");
    ESP_LOGI(TAG, "Chip:       ESP32-S3 rev %d", info.revision);
    ESP_LOGI(TAG, "Cores:      %d", info.cores);
    ESP_LOGI(TAG, "Flash:      %d MB", (int)(get_flash_size() / (1024 * 1024)));
    ESP_LOGI(TAG, "PSRAM:      %d KB", (int)(get_psram_size() / 1024));
    ESP_LOGI(TAG, "Free heap:  %d bytes", (int)esp_get_free_heap_size());
}

const char* BoardProfile::get_board_name() const { return "ESP32-S3-DevKitC-1 N16R8"; }
const char* BoardProfile::get_chip_model() const { return "ESP32-S3"; }
uint32_t BoardProfile::get_chip_revision() const { return 0; }
uint32_t BoardProfile::get_cpu_freq() const { return 240; }
uint32_t BoardProfile::get_core_count() const { return 2; }
size_t BoardProfile::get_flash_size() const { return 16 * 1024 * 1024; }
size_t BoardProfile::get_psram_size() const { return 8 * 1024 * 1024; }

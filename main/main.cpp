#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "io/board_profile.h"
#include "io/gpio_capability.h"
#include "io/gpio_bridge.h"

static const char *TAG = "vfpga";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== VFPGA Software-Defined Virtual FPGA ===");
    ESP_LOGI(TAG, "Target: ESP32-S3 N16R8 on DevKitC-1");

    BoardProfile board;
    board.detect();

    GpioCapability gpio_cap;
    gpio_cap.load_default();

    GpioBridge bridge;
    bridge.begin(&gpio_cap, &board);

    board.print_diagnostics();
    gpio_cap.print_status();

    ESP_LOGI(TAG, "VFPGA system initialized.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

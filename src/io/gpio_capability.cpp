#include "gpio_capability.h"
#include "esp_log.h"

static const char *TAG = "gpio_cap";

void GpioCapability::load_default() {
    for (size_t i = 0; i < GPIO_COUNT; ++i) {
        gpio_table_[i].gpio = i;
        gpio_table_[i].exposed = false;
        gpio_table_[i].input_capable = true;
        gpio_table_[i].output_capable = true;
        gpio_table_[i].reserved = true;
        gpio_table_[i].safe_default = false;
    }

    const uint8_t safe_gpios[] = {1,2,4,5,6,7,8,9,10,11,12,13,14,16,17,18,21,38,39,40,41,42};
    for (uint8_t g : safe_gpios) {
        gpio_table_[g].exposed = true;
        gpio_table_[g].reserved = false;
        gpio_table_[g].safe_default = true;
    }
}

bool GpioCapability::is_safe(uint8_t gpio) const {
    if (gpio >= GPIO_COUNT) return false;
    return gpio_table_[gpio].safe_default;
}

bool GpioCapability::is_reserved(uint8_t gpio) const {
    if (gpio >= GPIO_COUNT) return true;
    return gpio_table_[gpio].reserved;
}

const GpioInfo* GpioCapability::get_info(uint8_t gpio) const {
    if (gpio >= GPIO_COUNT) return nullptr;
    return &gpio_table_[gpio];
}

void GpioCapability::print_status() const {
    ESP_LOGI(TAG, "=== GPIO Capability Status ===");
    for (size_t i = 0; i < GPIO_COUNT; ++i) {
        const auto &g = gpio_table_[i];
        if (g.exposed || g.reserved) {
            const char *status = g.safe_default ? "SAFE" : (g.reserved ? "RESERVED" : "UNKNOWN");
            ESP_LOGI(TAG, "GPIO%2d: %s", (int)i, status);
        }
    }
}

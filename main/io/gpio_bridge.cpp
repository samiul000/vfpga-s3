#include "gpio_bridge.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "gpio_bridge";

void GpioBridge::begin(GpioCapability *cap, BoardProfile *board) {
    cap_ = cap;
    board_ = board;
    ESP_LOGI(TAG, "GPIO bridge initialized");
}

bool GpioBridge::map_input(uint16_t virtual_io, uint8_t physical_gpio) {
    if (!validate_mapping(virtual_io, physical_gpio)) return false;
    mappings_[virtual_io] = {physical_gpio, true, true};
    gpio_reset_pin(static_cast<gpio_num_t>(physical_gpio));
    gpio_set_direction(static_cast<gpio_num_t>(physical_gpio), GPIO_MODE_INPUT);
    return true;
}

bool GpioBridge::map_output(uint16_t virtual_io, uint8_t physical_gpio) {
    if (!validate_mapping(virtual_io, physical_gpio)) return false;
    mappings_[virtual_io] = {physical_gpio, false, true};
    gpio_reset_pin(static_cast<gpio_num_t>(physical_gpio));
    gpio_set_direction(static_cast<gpio_num_t>(physical_gpio), GPIO_MODE_OUTPUT);
    return true;
}

bool GpioBridge::unmap(uint16_t virtual_io) {
    if (virtual_io < MAX_VIO && mappings_[virtual_io].allocated) {
        mappings_[virtual_io] = {0, false, false};
        return true;
    }
    return false;
}

bool GpioBridge::validate_mapping(uint16_t virtual_io, uint8_t physical_gpio) {
    if (virtual_io >= MAX_VIO) return false;
    if (mappings_[virtual_io].allocated) return false;
    if (cap_ && !cap_->is_safe(physical_gpio)) {
        ESP_LOGE(TAG, "GPIO%d is not safe for VFPGA I/O", physical_gpio);
        return false;
    }
    return true;
}

void GpioBridge::sample_inputs() {
    for (size_t i = 0; i < MAX_VIO; ++i) {
        if (mappings_[i].allocated && mappings_[i].is_input) {
            gpio_get_level(static_cast<gpio_num_t>(mappings_[i].gpio));
        }
    }
}

void GpioBridge::commit_outputs() {
    for (size_t i = 0; i < MAX_VIO; ++i) {
        if (mappings_[i].allocated && !mappings_[i].is_input) {
            gpio_set_level(static_cast<gpio_num_t>(mappings_[i].gpio), 0);
        }
    }
}

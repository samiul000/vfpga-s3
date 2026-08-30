#pragma once

#include <cstdint>
#include <cstddef>

struct GpioInfo {
    uint8_t gpio;
    bool exposed;
    bool input_capable;
    bool output_capable;
    bool adc_capable;
    bool touch_capable;
    bool pwm_capable;
    bool usb_related;
    bool jtag_related;
    bool uart_related;
    bool strapping_pin;
    bool flash_related;
    bool psram_related;
    bool board_led;
    bool reserved;
    bool safe_default;
};

class GpioCapability {
public:
    void load_default();
    bool is_safe(uint8_t gpio) const;
    bool is_reserved(uint8_t gpio) const;
    const GpioInfo* get_info(uint8_t gpio) const;
    void print_status() const;

private:
    static constexpr size_t GPIO_COUNT = 49;
    GpioInfo gpio_table_[GPIO_COUNT]{};
};

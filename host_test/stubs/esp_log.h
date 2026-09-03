#pragma once
// Minimal ESP-IDF log stub for host-side CI compilation of the HDL toolchain.
// Only what src/hdl/*.cpp uses: ESP_LOGI / ESP_LOGW / ESP_LOGE / ESP_LOGD.
#include <cstdio>

#define ESP_LOGI(tag, format, ...) \
    printf("I (%s): " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) \
    printf("W (%s): " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) \
    printf("E (%s): " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) \
    printf("D (%s): " format "\n", tag, ##__VA_ARGS__)

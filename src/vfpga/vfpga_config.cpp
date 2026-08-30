#include "vfpga_config.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static const char *TAG = "vfpga_config";

bool VfpgaConfig::validate() const {
    if (magic != MAGIC) {
        ESP_LOGE(TAG, "Invalid magic: 0x%08X (expected 0x%08X)", magic, MAGIC);
        return false;
    }
    if (version != VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %d", version);
        return false;
    }
    uint16_t expected = compute_checksum();
    if (checksum != expected) {
        ESP_LOGE(TAG, "Checksum mismatch: 0x%04X (expected 0x%04X)", checksum, expected);
        return false;
    }
    return true;
}

uint16_t VfpgaConfig::compute_checksum() const {
    uint16_t sum = 0;
    const uint8_t *data = reinterpret_cast<const uint8_t*>(&magic);
    // Sum everything except checksum field itself (bytes 14-15)
    for (size_t i = 0; i < sizeof(VfpgaConfig); ++i) {
        if (i == 14 || i == 15) continue;
        sum += data[i];
    }
    return sum;
}

bool VfpgaConfig::save(const char *path) const {
    VfpgaConfig tmp = *const_cast<VfpgaConfig*>(this);
    tmp.checksum = tmp.compute_checksum();
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return false;
    }
    size_t written = fwrite(&tmp, sizeof(VfpgaConfig), 1, f);
    fclose(f);
    if (written != 1) {
        ESP_LOGE(TAG, "Write failed");
        return false;
    }
    ESP_LOGI(TAG, "Config saved to %s (%d bytes)", path, (int)sizeof(VfpgaConfig));
    return true;
}

bool VfpgaConfig::load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for reading", path);
        return false;
    }
    size_t read = fread(this, sizeof(VfpgaConfig), 1, f);
    fclose(f);
    if (read != 1) {
        ESP_LOGE(TAG, "Read failed");
        return false;
    }
    if (!validate()) return false;
    ESP_LOGI(TAG, "Config loaded: LUT=%d FF=%d BRAM=%d DSP=%d VIO=%d",
             lut_count, ff_count, bram_size, dsp_count, vio_count);
    return true;
}

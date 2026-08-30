#include "riscv_memory.h"
#include <cstring>
#include <esp_heap_caps.h>
#include "esp_log.h"

static const char *TAG = "riscv_mem";

void RiscvMemory::init(size_t size) {
    size_ = size;
    data_ = (uint8_t *)heap_caps_calloc(size, 1, MALLOC_CAP_INTERNAL);
    if (!data_) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", (int)size);
    }
}

void RiscvMemory::set_io_handler(void *ctx,
    uint32_t (*read)(void *ctx, uint32_t addr),
    void (*write)(void *ctx, uint32_t addr, uint32_t data))
{
    io_ctx_ = ctx;
    io_read_ = read;
    io_write_ = write;
}

bool RiscvMemory::is_mmio(uint32_t addr) const {
    return addr >= MMIO_BASE && addr < MMIO_BASE + MMIO_SIZE;
}

uint32_t RiscvMemory::read_word(uint32_t addr) const {
    if (is_mmio(addr)) {
        if (io_read_) return io_read_(io_ctx_, addr);
        return 0;
    }
    if (addr + 4 <= size_) {
        uint32_t val;
        memcpy(&val, data_ + addr, sizeof(uint32_t));
        return val;
    }
    return 0;
}

void RiscvMemory::write_word(uint32_t addr, uint32_t data) {
    if (is_mmio(addr)) {
        if (io_write_) io_write_(io_ctx_, addr, data);
        return;
    }
    if (addr + 4 <= size_) memcpy(data_ + addr, &data, sizeof(uint32_t));
}

uint8_t RiscvMemory::read_byte(uint32_t addr) const {
    if (is_mmio(addr)) {
        if (io_read_) return io_read_(io_ctx_, addr) & 0xFF;
        return 0;
    }
    if (addr < size_) return data_[addr];
    return 0;
}

void RiscvMemory::write_byte(uint32_t addr, uint8_t data) {
    if (is_mmio(addr)) {
        if (io_write_) io_write_(io_ctx_, addr, data);
        return;
    }
    if (addr < size_) data_[addr] = data;
}

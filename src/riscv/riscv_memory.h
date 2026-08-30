#pragma once

#include <cstdint>
#include <cstddef>

class RiscvMemory {
public:
    void init(size_t size);
    uint32_t read_word(uint32_t addr) const;
    void write_word(uint32_t addr, uint32_t data);
    uint8_t read_byte(uint32_t addr) const;
    void write_byte(uint32_t addr, uint8_t data);

    // Memory-mapped I/O callbacks
    void set_io_handler(void *ctx,
        uint32_t (*read)(void *ctx, uint32_t addr),
        void (*write)(void *ctx, uint32_t addr, uint32_t data));

private:
    static constexpr uint32_t MMIO_BASE = 0x10000000;
    static constexpr uint32_t MMIO_SIZE = 0x4000;

    uint8_t *data_ = nullptr;
    size_t size_ = 0;
    void *io_ctx_ = nullptr;
    uint32_t (*io_read_)(void *ctx, uint32_t addr) = nullptr;
    void (*io_write_)(void *ctx, uint32_t addr, uint32_t data) = nullptr;

    bool is_mmio(uint32_t addr) const;
};

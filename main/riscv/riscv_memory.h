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

private:
    uint8_t *data_ = nullptr;
    size_t size_ = 0;
};

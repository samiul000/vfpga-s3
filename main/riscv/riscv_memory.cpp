#include "riscv_memory.h"
#include <cstring>
#include <esp_heap_caps.h>

void RiscvMemory::init(size_t size) {
    size_ = size;
    data_ = (uint8_t *)heap_caps_calloc(size, 1, MALLOC_CAP_INTERNAL);
}

uint32_t RiscvMemory::read_word(uint32_t addr) const {
    if (addr + 4 <= size_) {
        uint32_t val;
        memcpy(&val, data_ + addr, sizeof(uint32_t));
        return val;
    }
    return 0;
}

void RiscvMemory::write_word(uint32_t addr, uint32_t data) {
    if (addr + 4 <= size_) memcpy(data_ + addr, &data, sizeof(uint32_t));
}

uint8_t RiscvMemory::read_byte(uint32_t addr) const {
    if (addr < size_) return data_[addr];
    return 0;
}

void RiscvMemory::write_byte(uint32_t addr, uint8_t data) {
    if (addr < size_) data_[addr] = data;
}

#include "vfpga_bram.h"
#include <cstring>
#include <esp_heap_caps.h>

void VBram::init(Size size) {
    size_ = size;
    capacity_ = static_cast<size_t>(size);
    data_ = (uint32_t *)heap_caps_calloc(capacity_, sizeof(uint32_t), MALLOC_CAP_INTERNAL);
    reset();
}

void VBram::reset() {
    if (data_) memset(data_, 0, capacity_ * sizeof(uint32_t));
}

uint32_t VBram::read(uint32_t addr) const {
    if (addr < capacity_) return data_[addr];
    return 0;
}

void VBram::write(uint32_t addr, uint32_t data) {
    if (addr < capacity_) data_[addr] = data;
}

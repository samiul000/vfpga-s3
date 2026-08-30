#pragma once

#include <cstdint>
#include <cstddef>

class VBram {
public:
    enum class Size { S64 = 64, S256 = 256, S1024 = 1024 };

    void init(Size size);
    void reset();
    uint32_t read(uint32_t addr) const;
    void write(uint32_t addr, uint32_t data);

private:
    Size size_ = Size::S64;
    uint32_t *data_ = nullptr;
    size_t capacity_ = 0;
};

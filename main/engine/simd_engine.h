#pragma once

#include <cstdint>

using VSignal = uint32_t;

class SimdEngine {
public:
    static void process_batch(const VSignal *a, const VSignal *b, VSignal *out, size_t count, uint8_t op);
};

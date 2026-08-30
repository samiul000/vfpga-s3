#include "simd_engine.h"

void SimdEngine::process_batch(const VSignal *a, const VSignal *b, VSignal *out, size_t count, uint8_t op) {
    for (size_t i = 0; i < count; ++i) {
        switch (op) {
            case 0: out[i] = a[i] & b[i]; break;
            case 1: out[i] = a[i] | b[i]; break;
            case 2: out[i] = a[i] ^ b[i]; break;
            default: out[i] = 0; break;
        }
    }
}

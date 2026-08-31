#pragma once

#include <cstdint>
#include <cstddef>

using VSignal = uint32_t;

struct CoreLut;

namespace vfpga_pie {

// ponytail: PIE-accelerated batch evaluation
// Process 4 LUTs simultaneously using Xtensa PIE 128-bit vector registers
// luts must be 16-byte aligned (use heap_caps_aligned_alloc)
void evaluate_batch_4(const CoreLut *luts, const VSignal *signals, VSignal *outputs, size_t count);

// Process 16 LUTs at a time (maximizes QR register usage)
void evaluate_batch_16(const CoreLut *luts, const VSignal *signals, VSignal *outputs, size_t count);

// Scalar fallback for non-aligned or remainder LUTs
void evaluate_scalar(const CoreLut *luts, const VSignal *signals, VSignal *outputs, size_t count);

} // namespace vfpga_pie

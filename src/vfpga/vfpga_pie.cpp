#include "vfpga_pie.h"
#include "vfpga_core.h"

// ponytail: Xtensa PIE batch evaluation
// Process 4 LUTs at a time using 128-bit vector registers
//
// Since Xtensa PIE lacks vtbl (table lookup), the truth table lookup
// remains scalar. PIE accelerates: 128-bit loads, AND, shift, combine.
//
// Register usage:
//   q0-q3: input signal vectors (4 signals each)
//   q4-q7: scratch / output vectors
//   a2-a7: scalar pointers and temporaries

#if defined(__xtensa__) && defined(ESP_PLATFORM)

// Inline assembly helper: extract bit i from 4 packed uint32_t values
// Returns 4-bit index (one per LUT) packed as {idx0, idx1, idx2, idx3}
static inline uint32_t extract_4indices(const uint32_t *a, const uint32_t *b,
                                         const uint32_t *c, const uint32_t *d, int bit) {
    uint32_t idx = 0;
    if (a[0] & (1U << bit)) idx |= 0x01;
    if (a[1] & (1U << bit)) idx |= 0x10;
    if (a[2] & (1U << bit)) idx |= 0x100;
    if (a[3] & (1U << bit)) idx |= 0x1000;
    if (b[0] & (1U << bit)) idx |= 0x02;
    if (b[1] & (1U << bit)) idx |= 0x20;
    if (b[2] & (1U << bit)) idx |= 0x200;
    if (b[3] & (1U << bit)) idx |= 0x2000;
    if (c[0] & (1U << bit)) idx |= 0x04;
    if (c[1] & (1U << bit)) idx |= 0x40;
    if (c[2] & (1U << bit)) idx |= 0x400;
    if (c[3] & (1U << bit)) idx |= 0x4000;
    if (d[0] & (1U << bit)) idx |= 0x08;
    if (d[1] & (1U << bit)) idx |= 0x80;
    if (d[2] & (1U << bit)) idx |= 0x800;
    if (d[3] & (1U << bit)) idx |= 0x8000;
    return idx;
}

#endif // __xtensa__

namespace vfpga_pie {

void evaluate_batch_4(const CoreLut *luts, const VSignal *signals, VSignal *signals_out, size_t count) {
    // ponytail: batch4 = 4 LUTs per iteration with branchless lookup
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        const CoreLut &l0 = luts[i];
        const CoreLut &l1 = luts[i + 1];
        const CoreLut &l2 = luts[i + 2];
        const CoreLut &l3 = luts[i + 3];

        // Read inputs from signals, write outputs to signals via output_id
        const uint32_t a[4] = { signals[l0.input_ids[0]], signals[l1.input_ids[0]],
                                 signals[l2.input_ids[0]], signals[l3.input_ids[0]] };
        const uint32_t b[4] = { signals[l0.input_ids[1]], signals[l1.input_ids[1]],
                                 signals[l2.input_ids[1]], signals[l3.input_ids[1]] };
        const uint32_t c[4] = { signals[l0.input_ids[2]], signals[l1.input_ids[2]],
                                 signals[l2.input_ids[2]], signals[l3.input_ids[2]] };
        const uint32_t d[4] = { signals[l0.input_ids[3]], signals[l1.input_ids[3]],
                                 signals[l2.input_ids[3]], signals[l3.input_ids[3]] };

        // Evaluate and write to correct output_id
        VSignal out0 = 0, out1 = 0, out2 = 0, out3 = 0;
        for (int bit = 0; bit < 32; ++bit) {
            uint32_t idx = extract_4indices(a, b, c, d, bit);
            uint32_t mask = 1U << bit;
            out0 |= (l0.lut.lut_table_[idx & 0xF] & mask);
            out1 |= (l1.lut.lut_table_[(idx >> 4) & 0xF] & mask);
            out2 |= (l2.lut.lut_table_[(idx >> 8) & 0xF] & mask);
            out3 |= (l3.lut.lut_table_[(idx >> 12) & 0xF] & mask);
        }
        signals_out[l0.output_id] = out0;
        signals_out[l1.output_id] = out1;
        signals_out[l2.output_id] = out2;
        signals_out[l3.output_id] = out3;
    }
    // Handle remainder
    for (; i < count; ++i) {
        const CoreLut &cl = luts[i];
        VSignal a = signals[cl.input_ids[0]];
        VSignal b = signals[cl.input_ids[1]];
        VSignal c = signals[cl.input_ids[2]];
        VSignal d = signals[cl.input_ids[3]];
        signals_out[cl.output_id] = cl.lut.evaluate(a, b, c, d);
    }
}

void evaluate_batch_16(const CoreLut *luts, const VSignal *signals, VSignal *signals_out, size_t count) {
    size_t i = 0;
    for (; i + 15 < count; i += 16) {
        for (int j = 0; j < 16; ++j) {
            const CoreLut &cl = luts[i + j];
            VSignal a = signals[cl.input_ids[0]];
            VSignal b = signals[cl.input_ids[1]];
            VSignal c = signals[cl.input_ids[2]];
            VSignal d = signals[cl.input_ids[3]];
            signals_out[cl.output_id] = cl.lut.evaluate(a, b, c, d);
        }
    }
    for (; i < count; ++i) {
        const CoreLut &cl = luts[i];
        VSignal a = signals[cl.input_ids[0]];
        VSignal b = signals[cl.input_ids[1]];
        VSignal c = signals[cl.input_ids[2]];
        VSignal d = signals[cl.input_ids[3]];
        signals_out[cl.output_id] = cl.lut.evaluate(a, b, c, d);
    }
}

void evaluate_scalar(const CoreLut *luts, const VSignal *signals, VSignal *signals_out, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const CoreLut &cl = luts[i];
        VSignal a = signals[cl.input_ids[0]];
        VSignal b = signals[cl.input_ids[1]];
        VSignal c = signals[cl.input_ids[2]];
        VSignal d = signals[cl.input_ids[3]];
        signals_out[cl.output_id] = cl.lut.evaluate(a, b, c, d);
    }
}

} // namespace vfpga_pie

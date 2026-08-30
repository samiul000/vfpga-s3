#include "vfpga_io.h"
#include <cstring>
#include <esp_heap_caps.h>

void VFpgaIo::init(size_t input_count, size_t output_count) {
    input_count_ = input_count;
    output_count_ = output_count;
    inputs_ = (VSignal *)heap_caps_calloc(input_count, sizeof(VSignal), MALLOC_CAP_INTERNAL);
    outputs_ = (VSignal *)heap_caps_calloc(output_count, sizeof(VSignal), MALLOC_CAP_INTERNAL);
}

VSignal VFpgaIo::read_input(uint16_t id) const {
    if (id < input_count_) return inputs_[id];
    return 0;
}

void VFpgaIo::write_input(uint16_t id, VSignal value) {
    if (id < input_count_) inputs_[id] = value;
}

VSignal VFpgaIo::read_output(uint16_t id) const {
    if (id < output_count_) return outputs_[id];
    return 0;
}

void VFpgaIo::write_output(uint16_t id, VSignal value) {
    if (id < output_count_) outputs_[id] = value;
}

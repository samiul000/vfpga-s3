#include "mapper.h"

bool Mapper::map_to_luts(const Netlist &netlist) {
    lut_count_ = netlist.component_count();
    return true;
}

size_t Mapper::lut_count() const { return lut_count_; }

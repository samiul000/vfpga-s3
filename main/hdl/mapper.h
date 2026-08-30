#pragma once

#include "netlist.h"

class Mapper {
public:
    bool map_to_luts(const Netlist &netlist);
    size_t lut_count() const;

private:
    size_t lut_count_ = 0;
};

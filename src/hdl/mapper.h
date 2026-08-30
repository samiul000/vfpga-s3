#pragma once

#include <cstdint>
#include <vector>
#include "netlist.h"

struct MappedLut {
    uint16_t id;
    uint16_t truth_table;
    std::vector<uint16_t> input_net_ids;
    uint16_t output_net_id;
};

struct MappedFf {
    uint16_t id;
    uint16_t d_net_id;
    uint16_t q_net_id;
};

struct MappedConfig {
    std::vector<MappedLut> luts;
    std::vector<MappedFf> ffs;
    std::vector<uint16_t> input_net_ids;
    std::vector<uint16_t> output_net_ids;
    std::vector<std::pair<uint16_t, uint32_t>> constants;
    uint16_t total_nets;
};

class Mapper {
public:
    MappedConfig map_to_luts(const Netlist &netlist);
    size_t lut_count() const;

private:
    uint16_t truth_table_for_op(const std::string &op) const;
};

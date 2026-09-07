#pragma once
// Host-only VCD writer (Stage 6). Standards-compliant subset: $date,
// $version, $timescale, $scope/$var/$upscope, $enddefinitions, #timestamps,
// scalar + vector changes. Deterministic var ids (§16). Header-only decl,
// impl in vcd.cpp.
#include <cstdint>
#include <string>
#include <vector>

#include "sim.h"

struct VcdSignal {
    uint16_t net = 0;          // single net, or first net of a bus member list
    std::string name;          // hierarchical name, e.g. "top.count"
    int width = 1;
    std::vector<uint16_t> bus_nets;  // empty unless width > 1; LSB-first nets
};

// Group watched nets into scalars + name[N] buses under scope "top".
// Nets starting with '_' go to scope "top.fabric".
std::vector<VcdSignal> vcd_signals(const Simulator &sim,
                                   const std::vector<uint16_t> &nets);

struct TraceSample;
bool write_vcd(const char *path, const char *timescale,
               const std::vector<VcdSignal> &sigs,
               const std::vector<TraceSample> &samples);

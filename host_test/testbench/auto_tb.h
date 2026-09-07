#pragma once
// Automatic/default testbench generation (Stage 12). Convenience only:
// waveform without invented assertions (§44-45). Decl here, impl below.
#include "tb.h"

class Netlist;
struct MappedConfig;

// cycles = sequential run length when a clock is detected.
Testbench auto_testbench(const Netlist &nl, const MappedConfig &cfg,
                         int cycles = 10);

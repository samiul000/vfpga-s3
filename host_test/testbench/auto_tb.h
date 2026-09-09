#pragma once
// Automatic/default testbench generation. Convenience only:
// waveform without invented assertions. Decl here, impl below.
#include "tb.h"

class Netlist;
struct MappedConfig;

// cycles = sequential run length when a clock is detected.
Testbench auto_testbench(const Netlist &nl, const MappedConfig &cfg,
                         int cycles = 10);

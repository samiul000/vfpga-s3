#pragma once

// Unified Virtual SoC demo: RV32I CPU drives the VFPGA fabric via MMIO.
// RISC-V program writes fabric inputs and reconfigures a LUT on the fly,
// reads back the evaluated output. Called from demo_run.cpp only.
void run_soc_demo();

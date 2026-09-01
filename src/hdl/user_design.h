#pragma once

#include <cstdint>
#include <vector>

using VSignal = uint32_t;

// Set to true to run hardware self-tests on boot
static const bool RUN_SELF_TESTS = false;

// ============================================================
//  USER HDL DESIGN : edit this string, re-flash, see results
//
//  Supported syntax: module, input, output, wire, register,
//  assign, always @(posedge clk), if/else, basic operators.
//  See HDL_GUIDE.md for full reference.
// ============================================================

static const char *USER_HDL =
    "module my_and;\n"
    "input a;\n"
    "input b;\n"
    "output y;\n"
    "assign y = a & b;\n"
    "endmodule\n";

// Clock cycles to run for sequential designs
static const int USER_CYCLES = 10;

// Custom test inputs (leave empty for auto-test).
// Each inner vector = one test case, values in alphabetical port order.
// Use 1 for high, 0 for low.
// Example for 2-input AND: { {0,0}, {0,1}, {1,0}, {1,1} }
static const std::vector<std::vector<VSignal>> USER_TEST_INPUTS = {};

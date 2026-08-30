#pragma once

// Auto-generated HDL file contents.
// To add a new design: add the .vhdl file, then paste its content here.

static const char *AND_GATE_HDL =
    "module and_gate;\n"
    "input a;\n"
    "input b;\n"
    "output y;\n"
    "assign y = a & b;\n"
    "endmodule\n";

static const char *COUNTER_HDL =
    "module counter;\n"
    "input clock;\n"
    "input reset;\n"
    "output [7:0] count;\n"
    "register [7:0] count_reg;\n"
    "always @(posedge clock) begin\n"
    "    if (reset)\n"
    "        count_reg <= 0;\n"
    "    else\n"
    "        count_reg <= count_reg + 1;\n"
    "end\n"
    "assign count = count_reg;\n"
    "endmodule\n";

static const char *LFSR_HDL =
    "module lfsr;\n"
    "input clock;\n"
    "input reset;\n"
    "output [7:0] state;\n"
    "register [7:0] lfsr_reg;\n"
    "always @(posedge clock) begin\n"
    "    if (reset)\n"
    "        lfsr_reg <= 255;\n"
    "    else\n"
    "        lfsr_reg <= lfsr_reg ^ 1;\n"
    "end\n"
    "assign state = lfsr_reg;\n"
    "endmodule\n";

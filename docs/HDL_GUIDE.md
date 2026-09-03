# VFPGA HDL Guide

How to write and run HDL designs on the ESP32-S3 virtual FPGA.

## Quick Start

1. Write a `.vhdl` file in the `hdl/` directory
2. Paste its content into `src/hdl/hdl_embedded.h` as a C string
3. Build: `pio run`
4. Flash: `pio run -t upload`
5. Monitor: `pio device monitor`

The HDL pipeline runs automatically on boot.

## Pipeline Architecture

```
.vhdl text
    |  Lexer (tokenize)
    v
  Tokens
    |  Parser (recursive descent)
    v
  AST (vector<AstNode>)
    |  Netlist::build_from_ast()
    v
  Netlist (nets + components)
    |  Mapper::map_to_luts()
    v
  MappedConfig (LUT truth tables, FF configs, routing)
    |  VFpgaCore::load_config()
    v
  VFPGA Core (LUTs + FFs wired up)
    |  evaluate_combinational() / clock()
    v
  Output signals
```

## Supported Syntax

### Module Structure

```vhdl
module name;
    input a;
    input b;
    output y;
    // declarations...
    // logic...
endmodule
```

### Port Declarations

```vhdl
input clock;            // 1-bit input
input [7:0] data;      // 8-bit input bus
output [3:0] result;   // 4-bit output bus
```

### Internal Signals

```vhdl
wire temp;              // 1-bit wire
wire [7:0] bus;        // 8-bit wire
register [7:0] count;  // 8-bit register (flip-flops)
```

### Combinational Logic

```vhdl
assign y = a & b;       // AND
assign y = a | b;       // OR
assign y = a ^ b;       // XOR
assign y = a + b;       // ADD (mapped to LUTs)
assign y = a;           // passthrough
```

### Sequential Logic

```vhdl
always @(posedge clock) begin
    if (reset)
        count <= 0;
    else
        count <= count + 1;
end
```

### Operators

| Operator | Meaning | LUT Truth Table |
|----------|---------|-----------------|
| `&`      | AND     | `0x8000`        |
| `\|`     | OR      | `0xFE00`        |
| `^`      | XOR     | `0x6969`        |
| `+`      | ADD     | `0x6666` (carry-less) |
| `==`     | EQUAL   | `0x8000`        |
| `!=`     | NOT EQ  | `0x7FFF`        |

### Number Literals

```vhdl
255         // decimal
8'hFF       // hex
8'b11111111 // binary
8'd255      // decimal with width
```

## HDL Compatibility Boundary

This HDL is a defined synthesizable subset, not full Verilog/VHDL. Anything outside this boundary is rejected by the parser.

### Supported

| Construct | Notes |
|-----------|-------|
| `module` / `endmodule` | Single module per design string |
| `input` / `output` (scalar + `[N:0]` buses) | Clock/reset auto-detected by port name |
| `wire` (scalar + buses) | Combinational nets |
| `register` (scalar + buses) | Maps to flip-flops |
| `assign` | Combinational continuous assignment |
| `always @(posedge clk)` | Single clocked block per register |
| `if` / `else` (inside `always`, single-statement) | Maps to MUX LUTs |
| Operators `& \| ^ + == !=` | See operator table above |
| Number literals (dec/hex/bin, sized) | See literals above |

### Unsupported (parser rejects or ignores)

| Construct | Reason |
|-----------|--------|
| `generate`, `parameter`, `localparam` | No elaboration phase |
| Tri-state (`z`, `inout`) | No multi-driver resolution in fabric |
| `#delays`, `@(negedge)`, multi-clock domains | No timing model; single posedge clock only |
| Multiple `always` blocks driving the same signal | Single-driver rule |
| `case`/`for`/`while`/`function`/`task` | Not in the subset (use `if`/`else` chains) |
| System tasks (`$display`, etc.) | No simulation backend |

### Known single-LUT architectural limits

- `+` maps to XOR LUTs **without a carry chain** — multi-bit addition does not propagate carries like silicon.
- Each LUT has 4 inputs; truth tables must be symmetric in the unused 4th input or it is tied to GND.
- Exhaustive auto-test covers designs with **<= 8 inputs** (2^N combinations); larger designs must set `USER_TEST_INPUTS`.
- Sequential auto-test finds `clock`/`reset` by port name and runs `USER_CYCLES` iterations.

### Verilog emission (AST to behavioral Verilog)

`src/hdl/verilog_emit.h` re-emits any in-subset design as clean, synthesizable Verilog (`VerilogEmitter::emit(name, ast)`): port lists are collected from declarations, `register` becomes `reg`, everything else passes through. Not wired into the firmware boot path; exercised on host and in CI:

- `host_test/test_hdl.cpp` — byte-exact golden emissions (AND, blinker, counter checks)
- `host_test/tb_*.v` — Icarus Verilog testbenches simulated in CI, diffed against fabric golden vectors
- `host_test/golden_counter.v` — synthesized in CI (`yosys synth -top counter`)

Known emission limits (shared with the parser, not emitter bugs): chained operators (`a & b & c`) collapse in the AST; multi-operand `if` conditions (`a == b`) are emitted as `1'b1` with a WARNING comment; `+` emits with carry while the fabric maps it carry-less — excluded from equivalence by design.

## Examples

### AND Gate (combinational)

```vhdl
module and_gate;
input a;
input b;
output y;

assign y = a & b;

endmodule
```

**What happens:**
- Lexer produces 18 tokens
- Parser creates AST: MODULE + INPUT(a) + INPUT(b) + OUTPUT(y) + ASSIGN(y, a & b)
- Netlist: 4 nets (a, b, y, and internal), 1 LUT4 component
- Mapper: LUT truth table = `0x8000` (AND), inputs wired to a, b
- VFPGA core: set a=1, b=1, evaluate, read y=1

### 8-bit Counter (sequential)

```vhdl
module counter;
input clock;
input reset;
output [7:0] count;

register [7:0] count_reg;

always @(posedge clock) begin
    if (reset)
        count_reg <= 0;
    else
        count_reg <= count_reg + 1;
end

assign count = count_reg;

endmodule
```

**What happens:**
- Lexer tokenizes: keywords, operators, numbers
- Parser: MODULE + INPUT(clock) + INPUT(reset) + OUTPUT(count[7:0]) + REGISTER(count_reg[7:0]) + ALWAYS_POSEDGE + IF(reset) + ASSIGN_LE + ASSIGN
- Netlist: 8-bit buses create 8 nets each for count and count_reg, 8 FF components, 8 LUT4 components for add
- Mapper: FFs for each bit, LUTs for increment logic
- VFPGA core: clock edge triggers FFs, combinational path computes next count

## Adding a New Design

### Step 1: Write the HDL

Open `src/hdl/user_design.h` and replace the `USER_HDL` string:

```cpp
static const char *USER_HDL =
    "module my_design;\n"
    "input clock;\n"
    "input [3:0] data;\n"
    "output [3:0] result;\n"
    "register [3:0] reg_a;\n"
    "always @(posedge clock) begin\n"
    "    reg_a <= data;\n"
    "end\n"
    "assign result = reg_a;\n"
    "endmodule\n";
```

### Step 2: Configure test options (optional)

In the same file, adjust `USER_CYCLES` and `USER_TEST_INPUTS` if needed:

```cpp
static const int USER_CYCLES = 10;  // clock cycles for sequential designs

// Custom test inputs (leave empty for auto-test)
static const std::vector<std::vector<VSignal>> USER_TEST_INPUTS = {};
```

### Step 3: Build and flash

```cpp
### Step 3: Build and flash

```bash
pio run -t upload --upload-port COM9
pio device monitor
```

The pipeline auto-detects your design type:
- **Combinational**: enumerates all 2^N input combinations (N ≤ 8)
- **Sequential**: finds `clock`/`reset` inputs, runs `USER_CYCLES` iterations

## API Reference

### Lexer

```cpp
#include "hdl/lexer.h"

Lexer lexer;
std::vector<Token> tokens = lexer.tokenize("module test; ...");
// Token: { type: TokenType::KW_MODULE, text: "module", line: 1 }
```

### Parser

```cpp
#include "hdl/parser.h"

Parser parser;
std::vector<AstNode> ast = parser.parse(tokens);
// AstNode: { type: AstNode::Type::MODULE, name: "test", ... }
```

### Netlist

```cpp
#include "hdl/netlist.h"

Netlist netlist;
netlist.build_from_ast(ast);

int16_t id = netlist.resolve("a");     // get net ID by name
size_t count = netlist.net_count();     // total nets
size_t comps = netlist.component_count(); // total components
```

### Mapper

```cpp
#include "hdl/mapper.h"

Mapper mapper;
MappedConfig cfg = mapper.map_to_luts(netlist);
// cfg.luts[i].truth_table, cfg.luts[i].input_net_ids, cfg.luts[i].output_net_id
// cfg.ffs[i].d_net_id, cfg.ffs[i].q_net_id
```

### VFPGA Core

```cpp
#include "vfpga/vfpga_core.h"

VFpgaCore core;
core.initialize();
core.load_config(cfg);

// Combinational
core.write_input(net_id, value);
core.evaluate_combinational();
VSignal out = core.read_output(net_id);

// Sequential
core.write_input(clk_net_id, 1);
core.run_cycles(1);
core.write_input(clk_net_id, 0);
core.run_cycles(1);
```

## Limitations

The mini-HDL is intentionally minimal. It does **not** support:

- Full Verilog/VHDL syntax
- `always @(*)` (combinational always blocks)
- Multi-driver nets
- Tri-state buffers
- `assign` inside `always` blocks
- Nested `if/else` (only one level)
- `case` statements
- Module instantiation
- Parameters/generics
- `#delay` timing

These are deliberate scope limits. The mini-HDL covers the core FPGA concepts: LUTs, FFs, combinational logic, sequential logic, and clock-driven state machines.

## Resource Limits

| Resource | Max |
|----------|-----|
| Signals  | 256 |
| LUTs     | 64  |
| FFs      | 64  |
| LUT inputs | 4 per LUT |

## Troubleshooting

**Build fails with "KW_IF is not a member"**
- The lexer.h is missing the `KW_IF` token type. Make sure all token types in the enum match what the parser uses.

**No output after flashing**
- Check serial monitor baud rate: 115200
- Press RESET button on the ESP32-S3

**Counter doesn't count**
- The always block clock input must be wired to a net
- Check that `run_hdl_pipeline` passes the correct net IDs for clock/reset

**LUT output wrong**
- Truth table values: AND=`0x8000`, OR=`0xFE00`, XOR=`0x6969`, PASS=`0xAAAA`

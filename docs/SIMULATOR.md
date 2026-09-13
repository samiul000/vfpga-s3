# VFPGA-S3 Host Simulator & Testbench Guide

Host-side FPGA learning workflow: HDL → compiler → netlist → testbench →
simulator → VCD → GTKWave. The ESP32 firmware is untouched; everything here
runs on the development computer.

## 1. Architecture

```text
User HDL
   │
   ▼
Existing HDL parser (src/hdl: lexer → parser → netlist → mapper)
   │
   ▼
Virtual VFPGA fabric (LUT4 truth tables, FF D→Q, routing = net ids)
   │
   ▼
Host simulator (host_test/simulator/sim.cpp)
   │
   ├── Testbench DSL (host_test/testbench) drives inputs / checks outputs
   ├── Tracer records changed signals only
   │
   ▼
VCD writer (host_test/waveform/vcd.cpp) → GTKWave
```

Key semantic (firmware parity): one ordered LUT pass per evaluation; nets
hold state between passes. Register feedback shares the LUT output net
(FF D == Q), so there is deliberately **no** iterate-to-stable loop
re-evaluating would oscillate by design. Missing LUT inputs read net 0,
exactly like `CoreLut` zero-init on firmware.

## 2. Testbench DSL

```text
testbench counter_tb {
    timescale 1ns;
    clock clk period=10ns;      # optional: duty=50 init=0
    reset reset active_high;    # or active_low (declares polarity)
    drive reset = 1;
    wait 20ns;
    drive reset = 0;
    repeat 10 {
        wait rising_edge(clk);  # or falling_edge(clk)
    }
    assert count == 10;         # ==,!=,<,>,<=,>=
    trace clk;                  # nets or bus bases
    stop;
}
```

Time is logical nanoseconds (default timescale 1 ns), unrelated to ESP32
frequency. Each rising edge of the first declared clock is one design cycle.
`trace` records from its declaration line on put traces near the top for a
full waveform. Failures print test name, time, signal, expected and actual
values, plus "did you mean" suggestions for unknown signals (all errors
carry line numbers).

## 3. Commands

```bash
vfpga compile <design>              # parse/map report → build/netlist/<base>.vnet
vfpga signals <design>              # inputs / outputs / registers
vfpga sim <design> [--cycles N] [--wave] [--open]
vfpga verify <design> [--tb f.tb] [--wave] [--open] [--verbose]
       [--report] [--fabric-debug] [--cycles N]
vfpga wave <file.vcd>               # open in GTKWave
```

Exit codes: 0 PASS, 1 verification failure, 2 compilation failure,
3 testbench syntax error, 4 simulation error, 5 GTKWave unavailable
(never fails an otherwise passing simulation).

Build the CLI (any C++17 compiler; CI uses Ubuntu g++):

```bash
g++ -std=c++17 -Wall -Isrc/hdl -Ihost_test/stubs \
  -Ihost_test/simulator -Ihost_test/waveform -Ihost_test/testbench \
  -Ihost_test/physical \
  tools/cli/main.cpp host_test/simulator/sim.cpp \
  host_test/testbench/tb.cpp host_test/testbench/tb_exec.cpp \
  host_test/testbench/auto_tb.cpp \
  host_test/waveform/vcd.cpp host_test/waveform/gtkwave.cpp \
  host_test/physical/physical_design.cpp \
  src/hdl/lexer.cpp src/hdl/parser.cpp \
  src/hdl/netlist.cpp src/hdl/mapper.cpp \
  -o build/vfpga
```

### Windows (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-gtkwave mingw-w64-ucrt-x86_64-clang
```

then build from a UCRT64 shell with `clang++` in place of `g++`,
adding `-static` to the link:

```bash
clang++ -std=c++17 -Wall -Isrc/hdl ... -static -o build/vfpga.exe
```

Two environment gotchas, both handled: stock MinGW.org GCC 6.3 is
pre-C++17 and cannot build this (use Clang or UCRT64 GCC), and a
dynamic link resolves `libstdc++-6.dll` from `C:\WINDOWS\SYSTEM32`
(where the old MinGW dropped its DLLs) ahead of MSYS2's static
linking sidesteps that. `vfpga wave` also probes the usual MSYS2 and
installer paths when `gtkwave` is not on PATH; adding
`C:\msys64\ucrt64\bin` to the Windows PATH is the durable fix.

## 4. Examples

Run these from the repo root (paths below are root-relative, and the
binary is `build/vfpga`). Inside a tutorial directory, the short form
`vfpga verify design.v --tb design.tb --wave --open` works instead —
see each tutorial's README for both variants.

```bash
vfpga compile examples/counter.v
vfpga signals examples/counter.v
vfpga sim examples/counter.v --wave
vfpga verify examples/counter.v --wave --open
vfpga verify examples/tutorial/06_counter/design.v \
    --tb examples/tutorial/06_counter/design.tb --wave --open
vfpga wave build/waves/counter.vcd
```

Tutorials `examples/tutorial/01_..08_`: and_gate, mux, half/full adder,
register, counter, shift register, toggle FSM each with
`design.v`, `design.tb`, `README.md`.

## 5. VCD & GTKWave

VCDs go to `build/waves/<design>.vcd` (deterministic `!`,`"`,`#`… ids,
change-only dump, `top` scope, `top.fabric` for internal nets).
Install GTKWave (`apt install gtkwave`, `brew install gtkwave`,
`pacman -S mingw-w64-ucrt-x86_64-gtkwave` on MSYS2, or
gtk-wave.mikekohn.net on Windows) and open with `vfpga wave` or
`--open`. Buses render as vectors; switch radix in GTKWave as needed.

## 6. Debugging

- `vfpga verify ... --verbose`: timestamped signal trace (edu mode).
- `vfpga sim ... --fabric-debug`: per-LUT inputs/mask/output, FF D/Q.
- `vfpga verify ... --report`: LUT4/FF/BRAM/DSP utilization.
- `vfpga signals`: discover net names before writing a testbench.

## 7. Limitations (functional simulation, NOT timing)

- No propagation/setup/hold/placement delays; no ESP32 cycle accuracy.
- BRAM/DSP have no netlist mapping yet (report shows 0).
- Bare `reg <= expr` binds the first declared register; use one
  register per always block or the if/else (MUX) path for multi-register
  designs (see tutorial 07).
- Auto testbenches generate waveforms only, never assertions.
- Cross-check against `verilog_emit` + Icarus where in doubt (CI does).

## 8. Known divergences from firmware (all covered by host tests)

1. **Single ordered LUT pass, no stabilize loop.** Register feedback
   shares the LUT output net (FF D == Q), so re-evaluation would
   oscillate by design; firmware's `run_cycles` is likewise one pass.
2. **Missing LUT inputs replicate input 0.** Firmware ties them to
   net 0, which mis-evaluates 2-input gates whenever net 0 disagrees
   (e.g. OR stuck at 1 when net 0 is 1). Replication is exact for every
   mapped truth table (idempotent 1–2 input ops, don't-care 4th input
   on XOR3/MAJ3, explicit GND on MUX).
3. **Whole nets + bit nets.** 1-bit ports exist as both `x` and `x[0]`;
   scalar logic drives whole nets, bus logic drives bit nets. The
   executor drives both and reads buses preferentially (scalars by LSB,
   since bitwise NOT yields `~v` over 32-bit signals).
4. **`!` (NOT) is wired through the parser** (`parse_unary_expr` sets
   `op = "!"`, mapped to the `0x5555` LUT; previously it silently parsed
   as PASS).

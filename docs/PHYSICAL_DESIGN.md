# ASIC-Style Physical Design (educational)

Same HDL runs through two backends: the VFPGA flow (LUT4/FF → ESP32-S3)
and an educational ASIC flow (standard cells → floorplan → place → route
→ layout DB). The ASIC flow is a **teaching model**: abstract units
(1u = 0.1 µm), estimated timing, Educational DRC. It is **not**
foundry-accurate and never claims to be. Real GDSII needs Yosys/OpenROAD
plus a real PDK (see §7).

## 1. Commands (repo root)

```bash
vfpga map <design> --target asic     # netlist -> standard cells
vfpga synth <design>                 # same, notes OpenROAD for real synth
vfpga floorplan|place <design> [--algorithm greedy_wirelength|row_pack|random_seeded] [--seed N] [--utilization F]
vfpga route|timing|congestion <design>
vfpga layout <design> [--output f] [--svg f]   # writes build/physical/<base>.layout.json
vfpga open build/physical/<base>.layout.json   # validate + summarize
vfpga report <design>                # VFPGA-vs-ASIC comparison table
vfpga build <design>                 # full flow + build/reports/<base>.txt
```

Full example:

```bash
vfpga build examples/physical/counter.v --svg
```

Exit codes: 0 ok, 2 compile fail, 4 physical error (mapping/routing/DRC).

## 2. Mapping

`&`→AND2, `|`→OR2, `^`→XOR2, `!`→INV, `MUX`→MUX2, `XOR3`/`MAJ3`→same,
`PASS`→BUF, FF→DFF. Multi-bit `+` arrives pre-decomposed as ripple-carry
(XOR3/MAJ3). Anything without a cell equivalent fails with
`ERROR [Physical:Mapping]` — never silently wrong.

## 3. Layout format

`build/physical/<design>.layout.json`, schema `vfpga-physical-layout`
version 1. Incompatible versions fail with a friendly error listing
supported versions. SVG export (`--svg`) renders die, core, cells (green
= DFF), M1/M2 routes, and vias for quick inspection without a viewer.

## 4. Reports

`timing` prints estimated critical path, WNS/TNS (10 ns clock
assumption), and clock-tree stats (sinks, buffers, depth). `congestion`
prints tile demand/capacity (max, average, overflow tiles). `report`
prints the VFPGA-vs-ASIC comparison table.

## 5. Limits

- One `top` module per file (parser emits single modules); instances are
  named `top/<output net>`.
- 2 metal layers (M1 horizontal, M2 vertical), L-routing with vias.
- No BRAM/DSP mapping yet (mapper emits LUT4/FF only).

## 5a. User CPU Designs

Any HDL design runs through the physical flow:

```bash
vfpga build my_cpu.v --svg
```

The ASIC backend maps operators to standard cells (`&` → AND2, etc.),
places them in rows, routes M1/M2, and exports layout JSON + SVG.
Designs with <= 8 inputs get exhaustive testing; larger designs need
manual testbenches.

For RISC-V-like CPUs, the block map overlays functional regions
(fetch, decode, alu, register_file, branch_unit, memory_interface, csr)
on the floorplan when the design structure matches. The block map is
derived from the RV32I opcode encoding — each LUT mapped to an opcode
class is assigned to its functional block.

Example: a simple ALU + register file design:

```vhdl
module simple_alu;
input clock;
input [2:0] op;
input [7:0] a, b;
output [7:0] result;
register [7:0] acc;
always @(posedge clock) begin
    if (op == 0) acc <= a + b;
    else if (op == 1) acc <= a & b;
    else if (op == 2) acc <= a | b;
end
assign result = acc;
endmodule
```

```bash
vfpga build simple_alu.v --svg
# -> build/physical/simple_alu.layout.json + .svg
```

## 6. Tests

`host_test/physical/test_physical.cpp` (CHECK-macro style, like
`test_hdl.cpp`): geometry, cell library, mapping (incl. decomposition),
placement legality, HPWL, full routing, timing, congestion, JSON
round-trip, version errors, seed determinism. CI job `sim-physical-test`.

## 7. Real backend (optional, not implemented here)

`vfpga asic gds` prints `Real GDSII backend unavailable` unless
Yosys/OpenROAD + a PDK are detected. OpenROAD scripts would live under
`tools/openroad/` with no PDK files in the repo. Never write a file
named `.gds` that is not real GDSII; the internal format is
`.layout.json`.

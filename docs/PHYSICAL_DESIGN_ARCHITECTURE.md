# Physical Design Architecture (audit, Phase 0)

Findings from inspecting the actual repository code. All paths are repo-relative.

## 1. HDL front end (existing, reused as-is)

- `src/hdl/lexer.h`: `Lexer::tokenize(source)` → `vector<Token>` (`type`, `text`, `line`).
- `src/hdl/parser.h`: `AstNode{type,name,op,children,child_idx,sub_nodes,bit_index,msb,lsb}`.
- `src/hdl/netlist.h`: `struct Net{id,name}`; `struct NetlistComponent{Type{LUT4,FF},id,op,inputs,output}`;
  `class Netlist` with `build_from_ast()`, `resolve()`, `input_names()/output_names()`.
  **Flat**: no modules, no instances, no hierarchy.
- `src/hdl/mapper.h`: `MappedLut{id,truth_table,input_net_ids,output_net_id}`,
  `MappedFf{id,d_net_id,q_net_id}`, `MappedConfig{luts,ffs,input_net_ids,output_net_ids,constants,total_nets}`.
  Op → truth table: `&`=0x8000, `|`=0xFFFE, `^`=0x6666, `PASS`=0xAAAA, `+`=0x6666,
  `==`=0x8000, `!=`=0x7FFF, `MUX`=0x00CA, `!`=0x5555, `XOR3`=0x9696, `MAJ3`=0xE8E8.
- `src/hdl/verilog_emit.h`: `VerilogEmitter::emit(module_name, ast)` → behavioral Verilog string.

## 2. VFPGA fabric (existing, untouched)

- `src/vfpga/vfpga_lut.h`: `VLut4{configure(tt), evaluate(a,b,c,d)}` with precomputed 16-entry table.
- `src/vfpga/vfpga_ff.h`: `VFlipFlop{reset(), clock_edge(d,enable), output()}`.
- `src/vfpga/vfpga_bram.h` / `vfpga_dsp.h` / `vfpga_mux.h`: primitives exist but are
  **not** produced by the mapper (only LUT4/FF).
- `src/vfpga/vfpga_core.h`: `VFpgaCore{init(), load_config(MappedConfig&), evaluate_combinational(), clock(), run_cycles()}`.

## 3. RISC-V CPU (existing, untouched)

- `src/riscv/riscv_cpu.h`: `RiscvCpu{reset(), step(), run(n), load_program(), get_pc(), get_reg()}`.
  Single class, decoder inline in `execute()` — **no sub-module hierarchy**.
- `src/riscv/riscv_memory.h`: 64 KB + MMIO at `0x10000000`.

## 4. Host simulator + testbench + VCD (existing, untouched)

- `host_test/simulator/sim.h`: `Simulator{load(nl,cfg), reset(), write_input(), read(), resolve(), eval_combinational(), clock(), step()}`.
- `host_test/simulator/trace.h`: `Tracer` (change-based `TraceSample{t,net,value}`).
- `host_test/testbench/tb.h`: `TbCmd` (10 command types), `parse_testbench()`.
- `host_test/testbench/tb_exec.h`: `TbExecutor`, `ExecResult{passed,failed,stopped,messages}`.
- `host_test/testbench/auto_tb.h`: `auto_testbench(nl, cfg, cycles)`.
- `host_test/waveform/vcd.h`: VCD writer; `gtkwave.h`: viewer launcher.
- `host_test/stubs/esp_log.h`: stubs `ESP_LOGI/W/E/D` → `printf`.

## 5. CLI (extended in place)

- `tools/cli/main.cpp`: flat if/else dispatch on `argv[1]` with helpers
  `load_design()`, `run_flow()`, `has_flag()`, `flag_value()`.
- New physical commands reuse this pattern (`cmd_physical()` dispatch table
  on `argv[2]`), keeping `main()` to one extra branch.

## 6. New ASIC backend (`host_test/physical/`, host-only)

Single-header-plus-implementation design (minimal files, no framework):

- `physical_design.h` / `physical_design.cpp`: unified IR + whole flow.
  - `ir.h` (folded in): `DesignIR` wraps `const Netlist&` + `const MappedConfig&`
    with derived instances (`IRInstance{cell_op, input_names, output_name, is_seq}`)
    and nets. Hierarchy: single `top` module (parser emits one module per file);
    instance names are `top/<output_net>`.
  - Geometry: integer grid units (`Pt{x,y}`, `Rect{x,y,w,h}`), Manhattan helpers. 1 unit = 0.1 µm.
  - Cell library: fixed table (W/H in units, pin offsets, comb/seq class, delay ps).
  - ASIC mapping: `NetlistComponent.op` → cell type (`&`→AND2, `|`→OR2, `^`→XOR2,
    `!`→INV, `MUX`→MUX2, `XOR3`→XOR3, `MAJ3`→MAJ3, FF/`DFF`→DFF; `+`/`==`/`!=` rejected
    with `ERROR [Physical:Mapping]` — never silently wrong).
  - Floorplan: die from `utilization` + `aspect`, core margin, row generation.
  - Placement: greedy wirelength + row packing, deterministic by seed.
  - Routing: BFS grid router on M1(H)/M2(V) + vias; Educational DRC error codes.
  - Clock tree: clock-net detect (name contains `clk`/`clock`), sink/buffer/depth report.
  - Timing: per-cell delay table → WNS/TNS/critical path (labeled Estimated).
  - Congestion: tile demand/capacity, max/avg/overflow.
  - Layout DB: `vfpga-physical-layout` JSON v1, `layout_export()`/`layout_import()`.
- `test_physical.cpp`: `CHECK`-macro tests mirroring `test_hdl.cpp` style.
- Viewer: headless SVG export (`--svg`) + `layout open` prints summary.
  No GUI framework; GTKWave-style interactivity is out of scope for the MVP.

## 7. Build / test (no new build system)

- Compiler: MSYS2 `mingw64` `clang++` (`C:\msys64\mingw64\bin\clang++.exe`), `-static` on Windows.
- CLI build appends the two physical sources to the existing command from `docs/SIMULATOR.md` §3.
- Tests compile the same way (`test_physical.cpp` + engine + hdl sources + stubs include).
- CI: new `sim-physical-test` job mirrors `sim-core-host-test`.
- Firmware untouched: nothing under `src/` added or modified.

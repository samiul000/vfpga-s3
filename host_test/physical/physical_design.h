// Educational ASIC-style physical design engine (host-only).
// Integer grid units throughout: 1 unit = 0.1 um. Deterministic by seed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mapper.h"
#include "netlist.h"

namespace physical {

struct Pt {
    int64_t x = 0, y = 0;
};
struct Rect {
    int64_t x = 0, y = 0, w = 0, h = 0;
};
int64_t manhattan(Pt a, Pt b);
bool overlaps(Rect a, Rect b);
bool contains(Rect a, Pt p);
Rect bbox(Pt a, Pt b);

// Standard-cell library entry (abstract educational units).
struct CellInfo {
    const char *type;
    int64_t w, h;
    bool sequential;
    int delay_ps;
};
const CellInfo *find_cell(const char *type);
size_t cell_lib_size();

// Unified netlist IR: one `top` module; instance per mapped component.
struct IRInstance {
    std::string name;  // top/<output net>
    std::string cell;  // AND2, DFF, ...
    std::vector<std::string> inputs;
    std::string output;
    bool sequential = false;
};
struct IRNet {
    std::string name;
    bool is_clock = false;
};
struct DesignIR {
    std::string top;
    std::vector<IRInstance> insts;
    std::vector<IRNet> nets;
};
// Returns false with `ERROR [Physical:Mapping]: ...` on unsupported ops.
bool build_ir(const Netlist &nl, const MappedConfig &cfg, DesignIR &ir,
              std::string &err);

struct PlaceOptions {
    double utilization = 0.6;
    double aspect = 1.0;
    int64_t margin = 40;
    int seed = 1;
    const char *algorithm = "greedy_wirelength";  // + row_pack, random_seeded
};
struct PlacedInst {
    std::string name, cell;
    int64_t x = 0, y = 0, w = 0, h = 0;
};
struct Floorplan {
    int64_t die_w = 0, die_h = 0;
    int64_t core_x = 0, core_y = 0, core_w = 0, core_h = 0;
    int64_t row_h = 30;
    int64_t rows = 0;
    std::vector<PlacedInst> insts;
};
bool floorplan_place(const DesignIR &ir, const PlaceOptions &opt,
                     Floorplan &fp, std::string &err);

struct RouteSeg {
    std::string net;
    int layer = 1;  // 1 = M1 horizontal, 2 = M2 vertical
    Pt a, b;
};
struct Via {
    std::string net;
    int from_layer = 1, to_layer = 2;
    Pt at;
};
struct RouteResult {
    std::vector<RouteSeg> segs;
    std::vector<Via> vias;
    std::vector<std::string> drc;  // Educational DRC violations, if any
    size_t unrouted = 0;
};
RouteResult route(const DesignIR &ir, const Floorplan &fp);

struct TimingResult {
    int64_t wns_ps = 0, tns_ps = 0, crit_ps = 0;  // Estimated
    std::vector<std::string> crit_path;
};
TimingResult estimate_timing(const DesignIR &ir, const Floorplan &fp);

struct Congestion {
    double max_use = 0.0, avg_use = 0.0;
    int overflow_tiles = 0;
};
Congestion analyze_congestion(const DesignIR &ir, const Floorplan &fp,
                              const RouteResult &rr);

int64_t total_hpwl(const DesignIR &ir, const Floorplan &fp);

struct ClockTree {
    std::string net;
    size_t sinks = 0, buffers = 0;
    int depth = 0;
};
ClockTree build_clock_tree(const DesignIR &ir);

std::string layout_export_json(const std::string &design, const DesignIR &ir,
                               const Floorplan &fp, const RouteResult &rr,
                               const TimingResult &tm);
// Validates `vfpga-physical-layout` v1; friendly error otherwise.
bool layout_import_check(const std::string &json, std::string &design_out,
                         std::string &err);
std::string layout_export_svg(const std::string &design, const DesignIR &ir,
                              const Floorplan &fp, const RouteResult &rr);

// RISC-V block map: maps opcode ranges to functional block names.
// Used to overlay labeled regions on the floorplan when the design
// structure matches a RISC-V-like CPU.
struct RiscvBlock {
    const char *name;      // "fetch", "decode", "alu", etc.
    const char *opcodes;   // comma-separated hex opcodes, "all", or "none"
    int area_weight;       // relative area for floorplan allocation
};
std::vector<RiscvBlock> riscv_block_map();

}  // namespace physical

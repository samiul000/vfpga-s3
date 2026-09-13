// Host tests for the educational physical-design engine.
#include <cstdio>
#include <string>

#include "lexer.h"
#include "mapper.h"
#include "netlist.h"
#include "parser.h"
#include "physical_design.h"

static int failures = 0;
#define CHECK(cond, msg)                          \
    do {                                          \
        if (cond) { printf("[PASS] %s\n", msg); } \
        else { printf("[FAIL] %s\n", msg); ++failures; } \
    } while (0)

static bool compile_src(const std::string &src, Netlist &nl,
                        MappedConfig &cfg) {
    Lexer lexer;
    std::vector<Token> toks = lexer.tokenize(src);
    Parser parser;
    std::vector<AstNode> ast = parser.parse(toks);
    nl.build_from_ast(ast);
    Mapper mapper;
    cfg = mapper.map_to_luts(nl);
    return true;
}

int main() {
    using namespace physical;

    // Geometry
    CHECK(manhattan(Pt{0, 0}, Pt{3, 4}) == 7, "manhattan distance");
    CHECK(overlaps(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}), "rect overlap");
    CHECK(!overlaps(Rect{0, 0, 10, 10}, Rect{20, 20, 5, 5}), "rect disjoint");
    CHECK(contains(Rect{0, 0, 10, 10}, Pt{5, 5}), "rect contains");

    // Cell library
    CHECK(cell_lib_size() >= 11, "cell library populated");
    CHECK(find_cell("AND2") && !find_cell("NOPE"), "cell lookup");
    const CellInfo *dff = find_cell("DFF");
    CHECK(dff && dff->sequential, "DFF is sequential");

    // Mapping: and_gate
    {
        Netlist nl;
        MappedConfig cfg;
        compile_src("module and_gate;\ninput a;\ninput b;\noutput y;\n"
                    "assign y = a & b;\nendmodule\n",
                    nl, cfg);
        DesignIR ir;
        std::string err;
        CHECK(build_ir(nl, cfg, ir, err), "and_gate maps to stdcells");
        CHECK(ir.insts.size() == 1 && ir.insts[0].cell == "AND2",
              "and_gate -> AND2");
    }

    // Mapping: register -> DFF; hierarchy naming top/<net>
    {
        Netlist nl;
        MappedConfig cfg;
        compile_src("module reg1;\ninput clock;\ninput d;\noutput q;\n"
                    "register r;\nassign q = r;\n"
                    "always @(posedge clock) begin\n r <= d;\nend\n"
                    "endmodule\n",
                    nl, cfg);
        DesignIR ir;
        std::string err;
        CHECK(build_ir(nl, cfg, ir, err), "register maps");
        bool has_dff = false, named = true;
        for (size_t i = 0; i < ir.insts.size(); ++i) {
            if (ir.insts[i].sequential) has_dff = true;
            if (ir.insts[i].name.compare(0, 4, "top/") != 0) named = false;
        }
        CHECK(has_dff, "register -> DFF present");
        CHECK(named, "instances named top/<net>");
    }

    // Mapping: multi-bit + arrives pre-decomposed as XOR3/MAJ3 ripple-carry
    {
        Netlist nl;
        MappedConfig cfg;
        compile_src("module add4;\ninput [3:0] a;\ninput [3:0] b;\n"
                    "output [3:0] s;\nassign s = a + b;\nendmodule\n",
                    nl, cfg);
        DesignIR ir;
        std::string err;
        CHECK(build_ir(nl, cfg, ir, err), "multi-bit + maps via decomposition");
        bool has_xor3 = false;
        for (size_t i = 0; i < ir.insts.size(); ++i)
            if (ir.insts[i].cell == "XOR3") has_xor3 = true;
        CHECK(has_xor3, "adder uses XOR3 sum cells");
    }

    // Floorplan + placement legality on counter
    {
        Netlist nl;
        MappedConfig cfg;
        compile_src("module counter;\ninput clock;\ninput reset;\n"
                    "output [3:0] count;\nregister [3:0] cnt;\n"
                    "always @(posedge clock) begin\n if (reset) cnt <= 0;\n "
                    "else cnt <= cnt + 1;\nend\nassign count = cnt;\n"
                    "endmodule\n",
                    nl, cfg);
        DesignIR ir;
        std::string err;
        CHECK(build_ir(nl, cfg, ir, err), "counter maps");
        PlaceOptions opt;
        Floorplan fp;
        CHECK(floorplan_place(ir, opt, fp, err), "counter floorplan+place");
        bool legal = true;
        Rect core{fp.core_x, fp.core_y, fp.core_w, fp.core_h};
        for (size_t i = 0; i < fp.insts.size() && legal; ++i) {
            Rect r{fp.insts[i].x, fp.insts[i].y, fp.insts[i].w,
                   fp.insts[i].h};
            if (!contains(core, Pt{r.x, r.y}) ||
                !contains(core, Pt{r.x + r.w, r.y + r.h}))
                legal = false;
            for (size_t j = i + 1; j < fp.insts.size(); ++j) {
                Rect q{fp.insts[j].x, fp.insts[j].y, fp.insts[j].w,
                       fp.insts[j].h};
                if (overlaps(r, q)) {
                    // Same-row adjacency (shared edge) is legal.
                    bool adjacent =
                        (r.x + r.w == q.x || q.x + q.w == r.x) &&
                        r.y == q.y;
                    if (!adjacent) legal = false;
                }
            }
        }
        CHECK(legal, "placement legal (inside core, no overlap)");
        CHECK(total_hpwl(ir, fp) > 0, "HPWL positive");
        RouteResult rr = route(ir, fp);
        CHECK(rr.unrouted == 0, "counter fully routed");
        TimingResult tm = estimate_timing(ir, fp);
        CHECK(tm.crit_ps > 0 && !tm.crit_path.empty(), "timing critical path");
        Congestion cg = analyze_congestion(ir, fp, rr);
        CHECK(cg.max_use >= 0.0, "congestion analyzed");
        ClockTree ct = build_clock_tree(ir);
        CHECK(ct.sinks > 0, "clock tree sinks found");
        std::string js = layout_export_json("counter", ir, fp, rr, tm);
        std::string dn, e2;
        CHECK(layout_import_check(js, dn, e2) && dn == "counter",
              "layout JSON round-trip");
        CHECK(!layout_import_check("{\"format\": \"x\", \"version\": 9}",
                                   dn, e2) &&
                  e2.find("Unsupported physical layout") != std::string::npos,
              "layout version error friendly");
        CHECK(layout_export_svg("counter", fp, rr).find("<svg") == 0,
              "SVG export");
    }

    // Determinism: same seed -> identical placement
    {
        Netlist nl;
        MappedConfig cfg;
        compile_src("module m;\ninput a;\ninput b;\ninput c;\noutput y;\n"
                    "assign y = a ^ b;\nendmodule\n",
                    nl, cfg);
        DesignIR ir;
        std::string err;
        build_ir(nl, cfg, ir, err);
        PlaceOptions o1, o2;
        o1.seed = 7;
        o2.seed = 7;
        Floorplan f1, f2;
        floorplan_place(ir, o1, f1, err);
        floorplan_place(ir, o2, f2, err);
        bool same = f1.insts.size() == f2.insts.size();
        for (size_t i = 0; i < f1.insts.size() && same; ++i)
            same = f1.insts[i].x == f2.insts[i].x &&
                   f1.insts[i].y == f2.insts[i].y;
        CHECK(same, "placement deterministic by seed");
    }

    if (failures == 0) printf("\nAll physical host tests PASS\n");
    else printf("\n%d physical test(s) FAILED\n", failures);
    return failures;
}

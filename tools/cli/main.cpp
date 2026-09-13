// vfpga host CLI: compile | sim | verify | signals | wave.
// Host-only; never linked into firmware (src/CMakeLists globs src/ only).

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "auto_tb.h"
#include "gtkwave.h"
#include "lexer.h"
#include "mapper.h"
#include "netlist.h"
#include "parser.h"
#include "physical_design.h"
#include "sim.h"
#include "tb.h"
#include "tb_exec.h"
#include "trace.h"
#include "vcd.h"

namespace {

std::string read_file(const char *path, bool &ok) {
    std::string s;
    ok = false;
    FILE *f = fopen(path, "rb");
    if (!f) return s;
    char buf[8192];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    ok = true;
    return s;
}

void ensure_dir(const char *dir) {
#ifdef _WIN32
    std::string cmd = std::string("mkdir \"") + dir + "\" 2>NUL";
#else
    std::string cmd = std::string("mkdir -p \"") + dir + "\" 2>/dev/null";
#endif
    (void)system(cmd.c_str());
}

std::string base_name(const char *path) {
    std::string p = path;
    size_t s = p.find_last_of("/\\");
    std::string b = (s == std::string::npos) ? p : p.substr(s + 1);
    size_t d = b.rfind('.');
    return (d == std::string::npos) ? b : b.substr(0, d);
}

int bus_width_of(const Netlist &nl, const std::string &name) {
    int w = 0;
    while (nl.resolve(name + "[" + std::to_string(w) + "]") >= 0) ++w;
    return w;
}

struct Design {
    Netlist nl;
    MappedConfig cfg;
    std::string base;
};

// Returns 0 ok, 2 compile fail.
int load_design(const char *path, Design &d) {
    bool ok = false;
    std::string src = read_file(path, ok);
    if (!ok) {
        printf("error: cannot read '%s'\n", path);
        return 2;
    }
    try {
        Lexer lexer;
        std::vector<Token> toks = lexer.tokenize(src);
        Parser parser;
        std::vector<AstNode> ast = parser.parse(toks);
        d.nl.build_from_ast(ast);
        Mapper mapper;
        d.cfg = mapper.map_to_luts(d.nl);
    } catch (const std::exception &e) {
        printf("error: compilation failed: %s\n", e.what());
        return 2;
    } catch (...) {
        printf("error: compilation failed (parse error)\n");
        return 2;
    }
    d.base = base_name(path);
    return 0;
}

void print_report(const Design &d) {
    printf("Resource Utilization\n");
    printf("----------------------------\n");
    printf("LUT4       %lu / 4096\n", (unsigned long)d.cfg.luts.size());
    printf("FF         %lu / 4096\n", (unsigned long)d.cfg.ffs.size());
    printf("BRAM       0 (not yet mapped)\n");
    printf("DSP        0 (not yet mapped)\n");
    printf("Signals    %lu\n", (unsigned long)d.nl.net_count());
}

void print_fabric_debug(const Design &d, Simulator &sim) {
    for (size_t i = 0; i < d.cfg.luts.size(); ++i) {
        const MappedLut &l = d.cfg.luts[i];
        printf("LUT%lu:\n  inputs =", (unsigned long)i);
        for (size_t k = 0; k < l.input_net_ids.size(); ++k)
            printf(" %u", l.input_net_ids[k]);
        printf("\n  LUT mask = 0x%04x\n  output = %u\n", l.truth_table,
               sim.read(l.output_net_id));
    }
    for (size_t i = 0; i < d.cfg.ffs.size(); ++i) {
        const MappedFf &f = d.cfg.ffs[i];
        printf("FF%lu:\n  D = %u\n  Q = %u\n", (unsigned long)i,
               sim.read(f.d_net_id), sim.read(f.q_net_id));
    }
}

void print_verbose(const Simulator &sim, const Tracer &tr) {
    // Educational trace: values grouped by timestamp.
    uint64_t last = 0;
    bool first = true;
    for (size_t i = 0; i < tr.samples().size(); ++i) {
        const TraceSample &s = tr.samples()[i];
        if (first || s.t != last) {
            printf("[%lu ns]\n", (unsigned long)s.t);
            last = s.t;
            first = false;
        }
        const char *nm = "?";
        for (size_t k = 0; k < sim.signal_count(); ++k)
            if (sim.signal(k).net_id == s.net) { nm = sim.signal(k).name.c_str(); break; }
        printf("  %s=%u\n", nm, s.value);
    }
}

bool has_flag(int argc, char **argv, const char *flag) {
    for (int i = 0; i < argc; ++i)
        if (std::string(argv[i]) == flag) return true;
    return false;
}

std::string flag_value(int argc, char **argv, const char *flag,
                       const char *def) {
    for (int i = 0; i + 1 < argc; ++i)
        if (std::string(argv[i]) == flag) return argv[i + 1];
    return def;
}

void usage() {
    printf("usage:\n");
    printf("  vfpga compile <design>\n");
    printf("  vfpga sim <design> [--cycles N] [--wave] [--open]\n");
    printf("  vfpga verify <design> [--tb file] [--wave] [--open] [--verbose]\n");
    printf("         [--report] [--fabric-debug] [--cycles N]\n");
    printf("  vfpga signals <design>\n");
    printf("  vfpga wave <file.vcd>\n");
    printf("  vfpga map <design> [--target asic|vfpga]\n");
    printf("  vfpga synth <design> [--target asic]\n");
    printf("  vfpga floorplan|place|route|timing|congestion <design>\n");
    printf("         [--algorithm greedy_wirelength|row_pack|random_seeded]\n");
    printf("         [--seed N] [--utilization F]\n");
    printf("  vfpga layout <design>|open <file> [--output f] [--svg f] [--open]\n");
    printf("  vfpga report <design>   (VFPGA-vs-ASIC comparison)\n");
    printf("  vfpga build <design>    (full ASIC flow: map..route+reports)\n");
}

// Educational ASIC flow. Returns 0 ok, 2 compile fail, 4 physical error.
int cmd_compile(const char *path);  // defined below
int cmd_physical(const char *sub, const char *design_path, int argc,
                 char **argv) {
    using namespace physical;
    std::string cmd = sub;
    // `layout open <file>` form: design_path is the layout file.
    if (cmd == "open" && design_path) {
        bool ok = false;
        std::string js = read_file(design_path, ok);
        if (!ok) {
            printf("error: cannot read '%s'\n", design_path);
            return 2;
        }
        std::string name, err;
        if (!layout_import_check(js, name, err)) {
            printf("%s\n", err.c_str());
            return 4;
        }
        printf("Layout: %s (%s)\n", name.c_str(), design_path);
        return 0;
    }
    if (!design_path) {
        usage();
        return 2;
    }
    Design d;
    int rc = load_design(design_path, d);
    if (rc) return rc;
    std::string target = flag_value(argc, argv, "--target", "asic");
    if (cmd == "map" && target == "vfpga") return cmd_compile(design_path);

    DesignIR ir;
    std::string err;
    if (!build_ir(d.nl, d.cfg, ir, err)) {
        printf("%s\n", err.c_str());
        return 4;
    }
    if (cmd == "map" || cmd == "synth") {
        size_t seq = 0;
        for (size_t i = 0; i < ir.insts.size(); ++i)
            if (ir.insts[i].sequential) ++seq;
        printf("ASIC mapping: %lu cells (%lu sequential)\n",
               (unsigned long)ir.insts.size(), (unsigned long)seq);
        if (cmd == "synth")
            printf("(educational mapping; use OpenROAD for real synthesis)\n");
        return 0;
    }
    PlaceOptions opt;
    std::string algo =
        flag_value(argc, argv, "--algorithm", "greedy_wirelength");
    opt.algorithm = algo.c_str();
    opt.seed = atoi(flag_value(argc, argv, "--seed", "1").c_str());
    opt.utilization = atof(flag_value(argc, argv, "--utilization", "0.6").c_str());
    Floorplan fp;
    if (!floorplan_place(ir, opt, fp, err)) {
        printf("%s\n", err.c_str());
        return 4;
    }
    if (cmd == "floorplan" || cmd == "place") {
        printf("Floorplan: die %lldx%lld, core %lldx%lld @(%lld,%lld), %lld rows\n",
               (long long)fp.die_w, (long long)fp.die_h,
               (long long)fp.core_w, (long long)fp.core_h,
               (long long)fp.core_x, (long long)fp.core_y,
               (long long)fp.rows);
        printf("Placement: %lu instances (%s, seed %d), HPWL %lld\n",
               (unsigned long)fp.insts.size(), opt.algorithm, opt.seed,
               (long long)total_hpwl(ir, fp));
        return 0;
    }
    RouteResult rr = route(ir, fp);
    TimingResult tm = estimate_timing(ir, fp);
    Congestion cg = analyze_congestion(ir, fp, rr);
    if (cmd == "route") {
        printf("Routing: %lu segs, %lu vias, %lu unrouted\n",
               (unsigned long)rr.segs.size(), (unsigned long)rr.vias.size(),
               (unsigned long)rr.unrouted);
        for (size_t i = 0; i < rr.drc.size(); ++i)
            printf("%s\n", rr.drc[i].c_str());
        return rr.unrouted ? 4 : 0;
    }
    if (cmd == "timing") {
        ClockTree ct = build_clock_tree(ir);
        printf("Estimated timing (ps): crit %lld, WNS %lld, TNS %lld\n",
               (long long)tm.crit_ps, (long long)tm.wns_ps,
               (long long)tm.tns_ps);
        printf("Critical path:");
        for (size_t i = 0; i < tm.crit_path.size(); ++i)
            printf(" %s", tm.crit_path[i].c_str());
        printf("\nClock '%s': %lu sinks, %lu buffers, depth %d\n",
               ct.net.c_str(), (unsigned long)ct.sinks,
               (unsigned long)ct.buffers, ct.depth);
        return 0;
    }
    if (cmd == "congestion") {
        printf("Congestion: max %.2f, avg %.2f, overflow tiles %d\n",
               cg.max_use, cg.avg_use, cg.overflow_tiles);
        return 0;
    }
    if (cmd == "report") {
        printf("================ IMPLEMENTATION COMPARISON ================\n");
        printf("Metric                 VFPGA                   ASIC\n");
        printf("Logic Resources        %lu LUT4               %lu cells\n",
               (unsigned long)d.cfg.luts.size(),
               (unsigned long)ir.insts.size());
        printf("Sequential Elements    %lu FFs                 %lu DFF\n",
               (unsigned long)d.cfg.ffs.size(),
               (unsigned long)build_clock_tree(ir).sinks);
        printf("Routing                Virtual Matrix          2 Metal Layers\n");
        printf("Area                   %lu Soft LUTs           %lld u^2 (die %lldx%lld)\n",
               (unsigned long)d.cfg.luts.size(),
               (long long)(fp.die_w * fp.die_h), (long long)fp.die_w,
               (long long)fp.die_h);
        printf("Est. Timing            ESP32 Exec Cycles       %lld ps crit\n",
               (long long)tm.crit_ps);
        printf("===========================================================\n");
        return 0;
    }
    // layout / build: export JSON (+SVG), write reports
    ensure_dir("build/physical");
    ensure_dir("build/reports");
    std::string out = flag_value(argc, argv, "--output", "");
    if (out.empty())
        out = std::string("build/physical/") + d.base + ".layout.json";
    std::string js = layout_export_json(d.base, ir, fp, rr, tm);
    FILE *f = fopen(out.c_str(), "wb");
    if (!f) {
        printf("error: cannot write '%s'\n", out.c_str());
        return 4;
    }
    fwrite(js.data(), 1, js.size(), f);
    fclose(f);
    std::string svg = flag_value(argc, argv, "--svg", "");
    if (!svg.empty() || has_flag(argc, argv, "--open")) {
        if (svg.empty())
            svg = std::string("build/physical/") + d.base + ".svg";
        std::string pic = layout_export_svg(d.base, fp, rr);
        FILE *g = fopen(svg.c_str(), "wb");
        if (g) {
            fwrite(pic.data(), 1, pic.size(), g);
            fclose(g);
        }
    }
    printf("Layout: %s (%lu insts, %lu segs, %lu vias)\n", out.c_str(),
           (unsigned long)fp.insts.size(), (unsigned long)rr.segs.size(),
           (unsigned long)rr.vias.size());
    if (cmd == "build") {
        char rp[256];
        snprintf(rp, sizeof(rp), "build/reports/%s.txt", d.base.c_str());
        FILE *h = fopen(rp, "wb");
        if (h) {
            fprintf(h, "design %s\ncells %lu\nhpwl %lld\ncrit_ps %lld\n"
                       "wns_ps %lld\ncong_max %.2f\nunrouted %lu\n",
                    d.base.c_str(), (unsigned long)ir.insts.size(),
                    (long long)total_hpwl(ir, fp), (long long)tm.crit_ps,
                    (long long)tm.wns_ps, cg.max_use,
                    (unsigned long)rr.unrouted);
            fclose(h);
        }
        printf("Report: %s\n", rp);
    }
    if (has_flag(argc, argv, "--open")) {
        int grc = open_wave(out.c_str());
        if (grc == 5)
            printf("(no viewer for layout files; JSON+SVG written)\n");
    }
    return rr.unrouted && cmd == "build" ? 4 : 0;
}

// Run tb (or auto tb), optionally write VCD. Returns ExecResult; sets vcd_path.
ExecResult run_flow(Design &d, Testbench *custom, int cycles, bool want_vcd,
                    std::string &vcd_path, Simulator &sim, Tracer &tr) {
    Testbench auto_tb;
    Testbench *tb = custom;
    if (!tb) {
        auto_tb = auto_testbench(d.nl, d.cfg, cycles);
        tb = &auto_tb;
    }
    sim.load(d.nl, d.cfg);
    TbExecutor ex(sim, tr);
    ExecResult r = ex.run(*tb);
    if (want_vcd) {
        ensure_dir("build/waves");
        vcd_path = "build/waves/" + d.base + ".vcd";
        std::vector<VcdSignal> sigs = vcd_signals(sim, tr.watched());
        if (!write_vcd(vcd_path.c_str(), "1ns", sigs, tr.samples())) {
            r.messages.push_back("error: VCD write failed");
            r.failed++;
        }
    }
    return r;
}

int cmd_compile(const char *design) {
    Design d;
    int rc = load_design(design, d);
    if (rc) return rc;
    printf("VFPGA Compiler\n\n");
    printf("Parsing ........ PASS\nNetlist ........ PASS\nMapping ........ PASS\n\n");
    printf("LUTs:  %lu\nFFs:   %lu\nBRAM:  0\nDSP:   0\n",
           (unsigned long)d.cfg.luts.size(), (unsigned long)d.cfg.ffs.size());
    ensure_dir("build/netlist");
    std::string out = "build/netlist/" + d.base + ".vnet";
    FILE *f = fopen(out.c_str(), "w");
    if (f) {
        fprintf(f, "# vfpga netlist summary for %s\n", d.base.c_str());
        fprintf(f, "nets %lu\n", (unsigned long)d.nl.net_count());
        for (size_t i = 0; i < d.cfg.luts.size(); ++i)
            fprintf(f, "lut %lu tt=0x%04x out=%u\n", (unsigned long)i,
                    d.cfg.luts[i].truth_table, d.cfg.luts[i].output_net_id);
        for (size_t i = 0; i < d.cfg.ffs.size(); ++i)
            fprintf(f, "ff %lu d=%u q=%u\n", (unsigned long)i,
                    d.cfg.ffs[i].d_net_id, d.cfg.ffs[i].q_net_id);
        fclose(f);
        printf("\nOutput:\n%s\n", out.c_str());
    }
    return 0;
}

int cmd_signals(const char *design) {
    Design d;
    int rc = load_design(design, d);
    if (rc) return rc;
    printf("Signals\n----------------------------\n\nInputs:\n");
    for (size_t i = 0; i < d.nl.input_names().size(); ++i) {
        const std::string &n = d.nl.input_names()[i];
        int w = bus_width_of(d.nl, n);
        printf("  %-12s %d bit%s\n", n.c_str(), w > 1 ? w : 1, w > 1 ? "s" : "");
    }
    printf("\nOutputs:\n");
    for (size_t i = 0; i < d.nl.output_names().size(); ++i) {
        const std::string &n = d.nl.output_names()[i];
        int w = bus_width_of(d.nl, n);
        printf("  %-12s %d bit%s\n", n.c_str(), w > 1 ? w : 1, w > 1 ? "s" : "");
    }
    printf("\nRegisters:\n");
    for (size_t i = 0; i < d.cfg.ffs.size(); ++i) {
        uint16_t q = d.cfg.ffs[i].q_net_id;
        const char *nm = "?";
        for (size_t k = 0; k < d.nl.net_count(); ++k)
            if (d.nl.get_net(k).id == q) { nm = d.nl.get_net(k).name.c_str(); break; }
        printf("  %s\n", nm);
    }
    return 0;
}

int cmd_verify(const char *design, int argc, char **argv, bool is_sim) {
    Design d;
    int rc = load_design(design, d);
    if (rc) return rc;
    std::string tb_path = flag_value(argc, argv, "--tb", "");
    bool want_vcd = has_flag(argc, argv, "--wave") || has_flag(argc, argv, "--open");
    bool want_open = has_flag(argc, argv, "--open");
    bool verbose = has_flag(argc, argv, "--verbose");
    bool report = has_flag(argc, argv, "--report");
    bool fabdbg = has_flag(argc, argv, "--fabric-debug");
    int cycles = atoi(flag_value(argc, argv, "--cycles", "10").c_str());

    Testbench custom;
    Testbench *tb_ptr = nullptr;
    if (!tb_path.empty()) {
        bool ok = false;
        std::string src = read_file(tb_path.c_str(), ok);
        if (!ok) {
            printf("error: cannot read testbench '%s'\n", tb_path.c_str());
            return 3;
        }
        std::vector<std::string> errors;
        if (!parse_testbench(src, tb_path.c_str(), custom, errors)) {
            for (size_t i = 0; i < errors.size(); ++i)
                printf("%s\n", errors[i].c_str());
            return 3;
        }
        tb_ptr = &custom;
    }

    Simulator sim;
    Tracer tr;
    std::string vcd_path;
    ExecResult r = run_flow(d, tb_ptr, cycles, want_vcd, vcd_path, sim, tr);

    if (is_sim) {
        printf("VFPGA Simulation\n\nDesign: %s\n", d.base.c_str());
        printf("Simulation time: %lu ns\n", (unsigned long)sim.time_ns());
        printf("\nSimulation %s\n", r.failed ? "FAIL" : "PASS");
    } else {
        printf("VFPGA Verification\n----------------------------\n\n");
        printf("Compile       PASS\nMapping       PASS\n");
        printf("Testbench     %s\n", tb_ptr ? "PASS" : "PASS (auto)");
        printf("Simulation    %s\n", r.failed ? "FAIL" : "PASS");
        printf("Assertions    %s (%d passed, %d failed)\n",
               r.failed ? "FAIL" : "PASS", r.passed, r.failed);
        for (size_t i = 0; i < r.messages.size(); ++i)
            printf("%s\n", r.messages[i].c_str());
        printf("\nResult: %s\n", r.failed ? "FAIL" : "PASS");
    }
    if (report) print_report(d);
    if (fabdbg) print_fabric_debug(d, sim);
    if (verbose) print_verbose(sim, tr);
    if (want_vcd && !vcd_path.empty()) {
        printf("\nWaveform:\n%s\n", vcd_path.c_str());
        if (want_open) {
            printf("\nLaunching GTKWave...\n");
            int grc = open_wave(vcd_path.c_str());
            if (grc == 5) return r.failed ? 1 : 0;  // sim PASS stands
            return grc == 0 ? (r.failed ? 1 : 0) : 4;
        }
    }
    return r.failed ? 1 : 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        return 2;
    }
    std::string cmd = argv[1];
    if (cmd == "compile" && argc >= 3) return cmd_compile(argv[2]);
    if (cmd == "signals" && argc >= 3) return cmd_signals(argv[2]);
    if ((cmd == "sim" || cmd == "verify") && argc >= 3)
        return cmd_verify(argv[2], argc, argv, cmd == "sim");
    if (cmd == "wave" && argc >= 3) {
        int rc = open_wave(argv[2]);
        if (rc == 0) printf("Opening %s in GTKWave\n", argv[2]);
        return rc;
    }
    if ((cmd == "map" || cmd == "synth" || cmd == "floorplan" ||
         cmd == "place" || cmd == "route" || cmd == "timing" ||
         cmd == "congestion" || cmd == "layout" || cmd == "report" ||
         cmd == "build" || cmd == "open") &&
        argc >= 3)
        return cmd_physical(argv[1], argv[2], argc, argv);
    usage();
    return 2;
}

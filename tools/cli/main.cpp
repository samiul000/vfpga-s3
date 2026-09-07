// vfpga host CLI: compile | sim | verify | signals | wave (§34-41).
// Host-only; never linked into firmware (src/CMakeLists globs src/ only).
// Exit codes (§52): 0 PASS, 1 verification failure, 2 compilation failure,
// 3 testbench syntax error, 4 simulation error, 5 GTKWave unavailable.
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
    usage();
    return 2;
}

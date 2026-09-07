// Host-side VCD tests (§17). Builds with CI g++ -std=c++17.
#include <cstdio>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "netlist.h"
#include "mapper.h"
#include "sim.h"
#include "trace.h"
#include "vcd.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("[PASS] %s\n", msg); } \
    else { printf("[FAIL] %s\n", msg); ++failures; } \
} while (0)

static std::string read_file(const char *path) {
    std::string s;
    FILE *f = fopen(path, "r");
    if (!f) return s;
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

int main() {
    Lexer lexer;
    auto tokens = lexer.tokenize(
        "module and_gate;\ninput a;\ninput b;\noutput y;\n"
        "assign y = a & b;\nendmodule\n");
    Parser parser;
    auto ast = parser.parse(tokens);
    Netlist nl;
    nl.build_from_ast(ast);
    Mapper mapper;
    MappedConfig cfg = mapper.map_to_luts(nl);
    Simulator sim;
    sim.load(nl, cfg);

    Tracer tr;
    tr.watch((uint16_t)sim.resolve("a"));
    tr.watch((uint16_t)sim.resolve("b"));
    tr.watch((uint16_t)sim.resolve("y"));
    SimTime t = 0;
    for (int av = 0; av < 2; ++av)
        for (int bv = 0; bv < 2; ++bv) {
            sim.write_input((uint16_t)sim.resolve("a"), (uint32_t)av);
            sim.write_input((uint16_t)sim.resolve("b"), (uint32_t)bv);
            sim.eval_combinational();
            tr.record(t, sim);
            t += 10;
        }

    // Change-only: 12 raw values -> fewer samples
    CHECK(tr.samples().size() < 12, "trace records changes only");
    CHECK(!tr.samples().empty(), "trace non-empty");

    std::vector<uint16_t> nets = tr.watched();
    std::vector<VcdSignal> sigs = vcd_signals(sim, nets);
    CHECK(sigs.size() == 3, "3 VCD signals");
    const char *path = "/tmp/vfpga_test.vcd";
    CHECK(write_vcd(path, "1ns", sigs, tr.samples()), "VCD written");

    std::string vcd = read_file(path);
    CHECK(vcd.find("$date") != std::string::npos, "VCD has $date");
    CHECK(vcd.find("$version") != std::string::npos, "VCD has $version");
    CHECK(vcd.find("$timescale 1ns $end") != std::string::npos, "VCD timescale");
    CHECK(vcd.find("$scope module top $end") != std::string::npos, "VCD scope");
    CHECK(vcd.find("$var wire 1 ! a $end") != std::string::npos, "VCD var a id !");
    CHECK(vcd.find("$var wire 1 \" b $end") != std::string::npos, "VCD var b id \"");
    CHECK(vcd.find("$var wire 1 # y $end") != std::string::npos, "VCD var y id #");
    CHECK(vcd.find("$enddefinitions $end") != std::string::npos, "VCD enddefinitions");
    CHECK(vcd.find("#0") != std::string::npos, "VCD timestamp #0");
    // No duplicate consecutive identical transitions (change-only dump)
    bool dup = false;
    std::string prev;
    size_t pos = 0;
    while (pos < vcd.size()) {
        size_t e = vcd.find('\n', pos);
        std::string line = vcd.substr(pos, e == std::string::npos ? e : e - pos);
        if (!line.empty() && (line[0] == '0' || line[0] == '1' || line[0] == 'b')) {
            if (line == prev) { dup = true; break; }
            prev = line;
        } else if (!line.empty()) {
            prev.clear();
        }
        if (e == std::string::npos) break;
        pos = e + 1;
    }
    CHECK(!dup, "no duplicate transitions for unchanged values");

    if (failures == 0) printf("\nAll VCD host tests PASS\n");
    else printf("\n%d VCD test(s) FAILED\n", failures);
    return failures;
}

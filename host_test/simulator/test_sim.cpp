// Host-side test for the simulator core (pipeline: HDL -> sim -> check).
// Compiles with host g++ -std=c++17 (CI); see .github/workflows/ci.yml.
#include <cstdio>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "netlist.h"
#include "mapper.h"
#include "sim.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("[PASS] %s\n", msg); } \
    else { printf("[FAIL] %s\n", msg); ++failures; } \
} while (0)

static void compile(const char *src, Netlist &netlist, MappedConfig &cfg) {
    Lexer lexer;
    auto tokens = lexer.tokenize(src);
    Parser parser;
    auto ast = parser.parse(tokens);
    netlist.build_from_ast(ast);
    Mapper mapper;
    cfg = mapper.map_to_luts(netlist);
}

int main() {
    // 1. AND gate truth table through the simulator
    {
        Netlist nl;
        MappedConfig cfg;
        compile("module and_gate;\ninput a;\ninput b;\noutput y;\n"
                "assign y = a & b;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        int16_t a = sim.resolve("a"), b = sim.resolve("b"), y = sim.resolve("y");
        CHECK(a >= 0 && b >= 0 && y >= 0, "AND nets resolve");
        bool ok = true;
        for (int av = 0; av < 2; ++av)
            for (int bv = 0; bv < 2; ++bv) {
                sim.write_input((uint16_t)a, (uint32_t)av);
                sim.write_input((uint16_t)b, (uint32_t)bv);
                sim.eval_combinational();
                if (sim.read((uint16_t)y) != (uint32_t)(av & bv)) ok = false;
            }
        CHECK(ok, "AND gate simulates correctly");
    }

    // 2. Counter: reset, then count 1..3 on successive clocks
    {
        Netlist nl;
        MappedConfig cfg;
        compile("module counter;\ninput clock;\ninput reset;\noutput [7:0] count;\n"
                "register [7:0] count_reg;\n"
                "always @(posedge clock) begin\n    if (reset) count_reg <= 0;\n"
                "    else count_reg <= count_reg + 1;\nend\n"
                "assign count = count_reg;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        int16_t reset = sim.resolve("reset");
        CHECK(reset >= 0, "Counter reset resolves");
        auto read_count = [&]() {
            uint32_t got = 0;
            for (int bit = 0; bit < 8; ++bit) {
                int16_t n = sim.resolve("count[" + std::to_string(bit) + "]");
                if (n >= 0 && sim.read((uint16_t)n)) got |= (1U << bit);
            }
            return got;
        };
        // reset asserted -> count 0 after a clock
        sim.write_input((uint16_t)reset, 1);
        sim.step();
        sim.eval_combinational();  // refresh output nets past the step
        CHECK(read_count() == 0, "Counter reset clears to 0");
        // release reset -> count 1..3 on successive clocks
        sim.write_input((uint16_t)reset, 0);
        bool ok = true;
        for (uint32_t expect = 1; expect <= 3; ++expect) {
            sim.step();
            sim.eval_combinational();
            uint32_t got = read_count();
            if (got != expect) {
                printf("[INFO] count=%u expected=%u\n", got, expect);
                ok = false;
            }
        }
        CHECK(ok, "Counter counts 1..3");
    }

    // 3. OR / XOR / NOT single-bit gates
    {
        Netlist nl;
        MappedConfig cfg;
        compile("module gates;\ninput a;\ninput b;\noutput o;\noutput x;\noutput n;\n"
                "assign o = a | b;\nassign x = a ^ b;\nassign n = !a;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        int16_t a = sim.resolve("a"), b = sim.resolve("b");
        int16_t o = sim.resolve("o"), x = sim.resolve("x"), n = sim.resolve("n");
        CHECK(a >= 0 && o >= 0 && x >= 0 && n >= 0, "Gate nets resolve");
        bool ok = true;
        for (int av = 0; av < 2; ++av)
            for (int bv = 0; bv < 2; ++bv) {
                sim.write_input((uint16_t)a, (uint32_t)av);
                sim.write_input((uint16_t)b, (uint32_t)bv);
                sim.eval_combinational();
                if (sim.read((uint16_t)o) != (uint32_t)(av | bv)) ok = false;
                if (sim.read((uint16_t)x) != (uint32_t)(av ^ bv)) ok = false;
                // NOT is bitwise (~) over 32-bit signals; scalars read by LSB.
                if ((sim.read((uint16_t)n) & 1) != (uint32_t)(!av)) ok = false;
            }
        CHECK(ok, "OR/XOR/NOT simulate correctly");
    }

    // 4. 4-bit adder (multi-bit arithmetic, ripple-carry single pass)
    {
        Netlist nl;
        MappedConfig cfg;
        compile("module add4;\ninput [3:0] a;\ninput [3:0] b;\noutput [3:0] s;\n"
                "assign s = a + b;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        auto drive_bus = [&](const char *name, uint32_t v) {
            for (int bit = 0; bit < 4; ++bit) {
                int16_t n = sim.resolve(std::string(name) + "[" + std::to_string(bit) + "]");
                if (n >= 0) sim.write_input((uint16_t)n, (v >> bit) & 1);
            }
        };
        auto read_bus = [&](const char *name) {
            uint32_t v = 0;
            for (int bit = 0; bit < 4; ++bit) {
                int16_t n = sim.resolve(std::string(name) + "[" + std::to_string(bit) + "]");
                if (n >= 0 && sim.read((uint16_t)n)) v |= (1U << bit);
            }
            return v;
        };
        drive_bus("a", 3); drive_bus("b", 5);
        sim.eval_combinational();
        bool ok = read_bus("s") == 8;
        drive_bus("a", 15); drive_bus("b", 1);
        sim.eval_combinational();
        ok = ok && read_bus("s") == 0;  // low 4 bits wrap
        CHECK(ok, "4-bit adder: 3+5=8, 15+1 wraps to 0");
    }

    // 5. D-register + 2:1 MUX (if/else) + 2-stage shift register
    {
        Netlist nl;
        MappedConfig cfg;
        compile("module mux2;\ninput clock;\ninput sel;\ninput a;\ninput b;\noutput y;\n"
                "register y_reg;\n"
                "always @(posedge clock) begin\n"
                "    if (sel) y_reg <= a;\n    else y_reg <= b;\nend\n"
                "assign y = y_reg;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        int16_t sel = sim.resolve("sel"), a = sim.resolve("a");
        int16_t b = sim.resolve("b"), y = sim.resolve("y");
        CHECK(sel >= 0 && y >= 0, "MUX nets resolve");
        bool ok = true;
        for (int sv = 0; sv < 2; ++sv)
            for (int av = 0; av < 2; ++av)
                for (int bv = 0; bv < 2; ++bv) {
                    sim.write_input((uint16_t)sel, (uint32_t)sv);
                    sim.write_input((uint16_t)a, (uint32_t)av);
                    sim.write_input((uint16_t)b, (uint32_t)bv);
                    sim.step();
                    sim.eval_combinational();
                    if (sim.read((uint16_t)y) != (uint32_t)(sv ? av : bv)) ok = false;
                }
        CHECK(ok, "2:1 MUX selects correctly");
    }
    {
        // Single D-register (one always block; the netlist binds a bare
        // ASSIGN_LE to the first declared register, so multi-register
        // shift chains need the IF/MUX path, covered by the counter test).
        Netlist nl;
        MappedConfig cfg;
        compile("module dreg;\ninput clock;\ninput d;\noutput q;\n"
                "register q_reg;\n"
                "always @(posedge clock) begin\n    q_reg <= d;\nend\n"
                "assign q = q_reg;\nendmodule\n",
                nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        int16_t d = sim.resolve("d"), q = sim.resolve("q");
        CHECK(d >= 0 && q >= 0, "D-register nets resolve");
        sim.write_input((uint16_t)d, 1);
        sim.step();
        sim.eval_combinational();
        bool ok = sim.read((uint16_t)q) == 1;
        sim.write_input((uint16_t)d, 0);
        sim.step();
        sim.eval_combinational();
        ok = ok && sim.read((uint16_t)q) == 0;
        CHECK(ok, "D-register captures input on clock");
    }

    if (failures == 0) printf("\nAll simulator core host tests PASS\n");
    else printf("\n%d simulator test(s) FAILED\n", failures);
    return failures;
}

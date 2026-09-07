// Testbench DSL + executor tests (§50): parsing, errors, execution,
// assertions, and trace->VCD wiring (stages 8-10). CI g++ -std=c++17.
#include <cstdio>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "netlist.h"
#include "mapper.h"
#include "sim.h"
#include "trace.h"
#include "tb.h"
#include "tb_exec.h"
#include "vcd.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("[PASS] %s\n", msg); } \
    else { printf("[FAIL] %s\n", msg); ++failures; } \
} while (0)

static bool compile_hdl(const char *src, Netlist &nl, MappedConfig &cfg) {
    Lexer lexer;
    auto tokens = lexer.tokenize(src);
    Parser parser;
    auto ast = parser.parse(tokens);
    nl.build_from_ast(ast);
    Mapper mapper;
    cfg = mapper.map_to_luts(nl);
    return true;
}

static bool parse_tb(const char *src, Testbench &tb,
                     std::vector<std::string> &errors) {
    return parse_testbench(src, "test.tb", tb, errors);
}

int main() {
    // 1. Parser: valid AND testbench
    {
        Testbench tb;
        std::vector<std::string> errors;
        bool ok = parse_tb(
            "testbench and_tb {\n"
            "    drive a = 0;\n    drive b = 0;\n    assert y == 0;\n"
            "    drive a = 1;\n    drive b = 1;\n    assert y == 1;\n"
            "    trace a;\n    trace y;\n}\n",
            tb, errors);
        CHECK(ok && tb.name == "and_tb" && tb.cmds.size() == 8, "valid DSL parses");
    }

    // 2. Parser errors carry line numbers
    {
        Testbench tb;
        std::vector<std::string> errors;
        CHECK(!parse_tb("testbench x {\n    frobnicate a;\n}\n", tb, errors) &&
              errors[0].find(":2:") != std::string::npos, "unknown command errors with line");
    }
    {
        Testbench tb;
        std::vector<std::string> errors;
        CHECK(!parse_tb("testbench x {\n    drive a = 1\n}\n", tb, errors),
              "missing semicolon errors");
    }
    {
        Testbench tb;
        std::vector<std::string> errors;
        CHECK(!parse_tb("testbench x {\n    wait soon;\n}\n", tb, errors),
              "invalid wait duration errors");
    }

    // 3. Execute AND vectors through the simulator
    {
        Netlist nl;
        MappedConfig cfg;
        compile_hdl("module and_gate;\ninput a;\ninput b;\noutput y;\n"
                    "assign y = a & b;\nendmodule\n",
                    nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        Tracer tr;
        Testbench tb;
        std::vector<std::string> errors;
        CHECK(parse_tb(
                  "testbench and_tb {\n"
                  "    drive a = 0;\n    drive b = 0;\n    assert y == 0;\n"
                  "    drive a = 0;\n    drive b = 1;\n    assert y == 0;\n"
                  "    drive a = 1;\n    drive b = 0;\n    assert y == 0;\n"
                  "    drive a = 1;\n    drive b = 1;\n    assert y == 1;\n"
                  "    trace a;\n    trace b;\n    trace y;\n}\n",
                  tb, errors),
              "AND tb parses");
        TbExecutor ex(sim, tr);
        ExecResult r = ex.run(tb);
        CHECK(r.failed == 0 && r.passed == 4, "AND vectors all pass");
    }

    // 4. Counter with clock/reset/edges + failing-assert diagnostics + VCD
    {
        Netlist nl;
        MappedConfig cfg;
        compile_hdl("module counter;\ninput clock;\ninput reset;\noutput [7:0] count;\n"
                    "register [7:0] count_reg;\n"
                    "always @(posedge clock) begin\n    if (reset) count_reg <= 0;\n"
                    "    else count_reg <= count_reg + 1;\nend\n"
                    "assign count = count_reg;\nendmodule\n",
                    nl, cfg);
        Simulator sim;
        sim.load(nl, cfg);
        Tracer tr;
        Testbench tb;
        std::vector<std::string> errors;
        CHECK(parse_tb(
                  "testbench counter_tb {\n"
                  "    timescale 1ns;\n"
                  "    clock clock period=10ns;\n"
                  "    reset reset active_high;\n"
                  "    trace clock;\n"
                  "    trace reset;\n"
                  "    trace count;\n"
                  "    drive reset = 1;\n"
                  "    wait 20ns;\n"
                  "    drive reset = 0;\n"
                  "    wait rising_edge(clock);\n"
                  "    assert count == 1;\n"
                  "    wait rising_edge(clock);\n"
                  "    assert count == 2;\n"
                  "    wait rising_edge(clock);\n"
                  "    assert count == 3;\n"
                  "}\n",
                  tb, errors),
              "counter tb parses");
        TbExecutor ex(sim, tr);
        ExecResult r = ex.run(tb);
        CHECK(r.failed == 0 && r.passed == 3, "counter reaches 1..3");

        std::vector<VcdSignal> sigs = vcd_signals(sim, tr.watched());
        CHECK(write_vcd("tb_counter.vcd", "1ns", sigs, tr.samples()),
              "counter VCD written");

        // failing assertion diagnostics
        Testbench tb2;
        CHECK(parse_tb("testbench bad_tb {\n    assert count == 99;\n}\n", tb2, errors),
              "bad tb parses");
        Tracer tr2;
        TbExecutor ex2(sim, tr2);
        ExecResult r2 = ex2.run(tb2);
        CHECK(r2.failed == 1 && !r2.messages.empty() &&
              r2.messages[0].find("[FAIL]") != std::string::npos &&
              r2.messages[0].find("count") != std::string::npos &&
              r2.messages[0].find("99") != std::string::npos,
              "failing assert reports signal/expected/actual");
    }

    if (failures == 0) printf("\nAll testbench host tests PASS\n");
    else printf("\n%d testbench test(s) FAILED\n", failures);
    return failures;
}

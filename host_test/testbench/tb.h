#pragma once
// Host-only testbench DSL lexer/parser (Stage 8). Small deterministic
// language (§18-20); parses to a command list, never executes raw text (§25).
// Errors carry line numbers (§26). Header-only decl, impl in tb.cpp.
#include <cstdint>
#include <string>
#include <vector>

struct TbCmd {
    enum class Type {
        TIMESCALE,
        CLOCK,       // name=net, period_ns, duty_pct, init
        RESET,       // name=net, active_high
        DRIVE,       // name=net, value
        WAIT_NS,     // duration_ns
        WAIT_EDGE,   // name=net, rising?
        REPEAT,      // count + block
        ASSERT,      // name=net-or-bus, op, value
        TRACE,       // name=net-or-bus
        STOP
    };
    Type type = Type::STOP;
    std::string name;
    std::string op;          // assert operator: ==,!=,<,>,<=,>=
    uint64_t value = 0;      // drive/assert value, wait ns, repeat count
    uint64_t period_ns = 10;
    int duty_pct = 50;
    int init = 0;
    bool active_high = true;
    bool rising = true;
    std::vector<TbCmd> block;  // REPEAT body
    size_t line = 0;
};

struct Testbench {
    std::string name;
    std::vector<TbCmd> cmds;
};

// Parse DSL source; returns true on success. On failure returns false and
// appends "path:line: error: ..." messages to errors.
bool parse_testbench(const std::string &src, const char *path,
                     Testbench &tb, std::vector<std::string> &errors);

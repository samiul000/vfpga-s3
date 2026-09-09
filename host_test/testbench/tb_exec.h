#pragma once
#include <string>
#include <vector>

#include "sim.h"
#include "tb.h"
#include "trace.h"

struct ExecResult {
    int passed = 0;
    int failed = 0;
    bool stopped = false;
    std::vector<std::string> messages;  // FAIL lines + errors
};

class TbExecutor {
public:
    TbExecutor(Simulator &sim, Tracer &tr) : sim_(sim), tr_(tr) {}

    ExecResult run(const Testbench &tb);

private:
    struct Clock {
        uint16_t net = 0;
        uint64_t period = 10;
        uint64_t low_ns = 5, high_ns = 5;
        int level = 0;
        uint64_t next_toggle = 0;
    };

    Simulator &sim_;
    Tracer &tr_;
    std::vector<Clock> clocks_;
    size_t primary_ = 0;

    bool resolve_one(const std::string &name, uint16_t &net);
    bool bus_nets(const std::string &base, std::vector<uint16_t> &nets);
    uint32_t read_value(const std::string &name, bool &is_bus);
    void write_value(const std::string &name, uint64_t v, ExecResult &r, size_t line);
    void suggest(const std::string &name, ExecResult &r, size_t line);
    void advance_to(uint64_t t_end, ExecResult &r);
    void exec_list(const std::vector<TbCmd> &cmds, ExecResult &r);
};

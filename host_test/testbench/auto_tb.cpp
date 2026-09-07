// Auto testbench generation. See auto_tb.h.
#include "auto_tb.h"

#include "mapper.h"
#include "netlist.h"

namespace {

const char *find_input(const Netlist &nl, const char *a, const char *b) {
    for (size_t i = 0; i < nl.input_names().size(); ++i) {
        const std::string &n = nl.input_names()[i];
        if (n == a || n == b) return n.c_str();
    }
    return nullptr;
}

TbCmd drive_cmd(const std::string &name, uint64_t v, size_t line) {
    TbCmd c;
    c.type = TbCmd::Type::DRIVE;
    c.name = name;
    c.value = v;
    c.line = line;
    return c;
}

TbCmd trace_cmd(const std::string &name, size_t line) {
    TbCmd c;
    c.type = TbCmd::Type::TRACE;
    c.name = name;
    c.line = line;
    return c;
}

}  // namespace

Testbench auto_testbench(const Netlist &nl, const MappedConfig &cfg,
                         int cycles) {
    Testbench tb;
    tb.name = "auto_tb";
    size_t ln = 1;

    // trace everything observable
    std::vector<std::string> io = nl.input_names();
    for (size_t i = 0; i < nl.output_names().size(); ++i)
        io.push_back(nl.output_names()[i]);

    bool sequential = !cfg.ffs.empty();
    const char *clk = find_input(nl, "clock", "clk");
    const char *rst = find_input(nl, "reset", "rst");

    if (sequential && clk) {
        TbCmd ck;
        ck.type = TbCmd::Type::CLOCK;
        ck.name = clk;
        ck.period_ns = 10;
        ck.line = ln++;
        tb.cmds.push_back(ck);
        if (rst) {
            TbCmd rc;
            rc.type = TbCmd::Type::RESET;
            rc.name = rst;
            rc.active_high = true;
            rc.line = ln++;
            tb.cmds.push_back(rc);
            tb.cmds.push_back(drive_cmd(rst, 1, ln++));
            TbCmd w;
            w.type = TbCmd::Type::WAIT_NS;
            w.value = 20;
            w.line = ln++;
            tb.cmds.push_back(w);
            tb.cmds.push_back(drive_cmd(rst, 0, ln++));
        }
        TbCmd rep;
        rep.type = TbCmd::Type::REPEAT;
        rep.value = (uint64_t)(cycles > 0 ? cycles : 10);
        rep.line = ln++;
        TbCmd edge;
        edge.type = TbCmd::Type::WAIT_EDGE;
        edge.name = clk;
        edge.rising = true;
        edge.line = ln;
        rep.block.push_back(edge);
        tb.cmds.push_back(rep);
    } else {
        // combinational: exhaustive vectors for small 1-bit input sets
        std::vector<std::string> scalars;
        for (size_t i = 0; i < nl.input_names().size(); ++i) {
            const std::string &n = nl.input_names()[i];
            if (nl.resolve(n + "[0]") < 0) scalars.push_back(n);
        }
        if (scalars.size() <= 8 && !scalars.empty()) {
            uint64_t nvec = 1ULL << scalars.size();
            for (uint64_t v = 0; v < nvec; ++v)
                for (size_t i = 0; i < scalars.size(); ++i)
                    tb.cmds.push_back(drive_cmd(scalars[i], (v >> i) & 1, ln++));
        } else {
            for (size_t i = 0; i < scalars.size(); ++i)
                tb.cmds.push_back(drive_cmd(scalars[i], 0, ln++));
            for (size_t i = 0; i < scalars.size(); ++i)
                tb.cmds.push_back(drive_cmd(scalars[i], 1, ln++));
        }
    }

    for (size_t i = 0; i < io.size(); ++i)
        tb.cmds.push_back(trace_cmd(io[i], ln++));
    return tb;
}

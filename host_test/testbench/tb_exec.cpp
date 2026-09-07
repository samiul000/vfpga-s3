// Testbench executor. See tb_exec.h.
#include "tb_exec.h"

#include <cstdio>

namespace {

bool check_op(const std::string &op, uint32_t actual, uint64_t expected) {
    if (op == "==") return actual == expected;
    if (op == "!=") return actual != expected;
    if (op == "<") return actual < expected;
    if (op == ">") return actual > expected;
    if (op == "<=") return actual <= expected;
    if (op == ">=") return actual >= expected;
    return false;
}

}  // namespace

bool TbExecutor::resolve_one(const std::string &name, uint16_t &net) {
    int16_t id = sim_.resolve(name);
    if (id < 0) return false;
    net = (uint16_t)id;
    return true;
}

bool TbExecutor::bus_nets(const std::string &base, std::vector<uint16_t> &nets) {
    nets.clear();
    for (int bit = 0; ; ++bit) {
        int16_t id =
            sim_.resolve(base + "[" + std::to_string(bit) + "]");
        if (id < 0) break;
        nets.push_back((uint16_t)id);
        if (bit > 64) break;
    }
    return !nets.empty();
}

uint32_t TbExecutor::read_value(const std::string &name, bool &is_bus) {
    uint16_t net = 0;
    if (resolve_one(name, net)) {
        // A bare bus base (e.g. "count") may also resolve as a whole net;
        // prefer per-bit assembly when bit nets exist.
        std::vector<uint16_t> bits;
        if (bus_nets(name, bits)) {
            is_bus = true;
            uint32_t v = 0;
            for (size_t i = 0; i < bits.size() && i < 32; ++i)
                if (sim_.read(bits[i])) v |= (1U << i);
            return v;
        }
        is_bus = false;
        return sim_.read(net);
    }
    std::vector<uint16_t> bits;
    if (bus_nets(name, bits)) {
        is_bus = true;
        uint32_t v = 0;
        for (size_t i = 0; i < bits.size() && i < 32; ++i)
            if (sim_.read(bits[i])) v |= (1U << i);
        return v;
    }
    is_bus = false;
    return 0;
}

void TbExecutor::suggest(const std::string &name, ExecResult &r, size_t line) {
    char buf[256];
    snprintf(buf, sizeof(buf), "error line %lu: unknown signal '%s'",
             (unsigned long)line, name.c_str());
    std::string msg = buf;
    // cheap "did you mean": substring match, up to 3 candidates
    std::string cands;
    int n = 0;
    for (size_t i = 0; i < sim_.signal_count() && n < 3; ++i) {
        const std::string &s = sim_.signal(i).name;
        if (s.find(name) != std::string::npos || name.find(s) != std::string::npos) {
            if (n > 0) cands += ", ";
            cands += s;
            ++n;
        }
    }
    if (!cands.empty()) msg += "\nDid you mean: " + cands;
    r.messages.push_back(msg);
    r.failed++;
}

void TbExecutor::write_value(const std::string &name, uint64_t v,
                             ExecResult &r, size_t line) {
    uint16_t net = 0;
    std::vector<uint16_t> bits;
    if (bus_nets(name, bits)) {
        for (size_t i = 0; i < bits.size(); ++i)
            sim_.write_input(bits[i], (uint32_t)((v >> i) & 1));
    } else if (resolve_one(name, net)) {
        sim_.write_input(net, (uint32_t)v);
    } else {
        suggest(name, r, line);
        return;
    }
    sim_.eval_combinational();
    tr_.record(sim_.time_ns(), sim_);
}

void TbExecutor::advance_to(uint64_t t_end, ExecResult &r) {
    (void)r;
    for (;;) {
        // next clock toggle
        uint64_t t_next = t_end + 1;
        for (size_t i = 0; i < clocks_.size(); ++i)
            if (clocks_[i].next_toggle < t_next) t_next = clocks_[i].next_toggle;
        if (t_next > t_end) break;
        sim_.advance(t_next - sim_.time_ns());
        for (size_t i = 0; i < clocks_.size(); ++i) {
            Clock &c = clocks_[i];
            if (c.next_toggle != t_next) continue;
            c.level = !c.level;
            c.next_toggle += c.level ? c.high_ns : c.low_ns;
            sim_.write_input(c.net, (uint32_t)c.level);
        }
        sim_.eval_combinational();
        tr_.record(sim_.time_ns(), sim_);
        // rising edge of primary clock = one design cycle (§27-28)
        for (size_t i = 0; i < clocks_.size(); ++i) {
            if (i == primary_ && clocks_[i].level == 1 &&
                clocks_[i].next_toggle - clocks_[i].high_ns == t_next) {
                sim_.step();
                sim_.eval_combinational();
                tr_.record(sim_.time_ns(), sim_);
            }
        }
    }
    sim_.advance(t_end - sim_.time_ns());
    sim_.eval_combinational();
    tr_.record(sim_.time_ns(), sim_);
}

void TbExecutor::exec_list(const std::vector<TbCmd> &cmds, ExecResult &r) {
    for (size_t k = 0; k < cmds.size() && !r.stopped; ++k) {
        const TbCmd &c = cmds[k];
        switch (c.type) {
            case TbCmd::Type::TIMESCALE:
                break;  // durations are ns-only (§9 default 1ns)
            case TbCmd::Type::CLOCK: {
                uint16_t net = 0;
                if (!resolve_one(c.name, net)) { suggest(c.name, r, c.line); break; }
                Clock ck;
                ck.net = net;
                ck.period = c.period_ns ? c.period_ns : 10;
                ck.low_ns = (ck.period * (uint64_t)(100 - c.duty_pct)) / 100;
                ck.high_ns = ck.period - ck.low_ns;
                if (!ck.low_ns) ck.low_ns = 1;
                if (!ck.high_ns) ck.high_ns = 1;
                ck.level = c.init ? 1 : 0;
                ck.next_toggle = ck.level ? ck.high_ns : ck.low_ns;
                sim_.write_input(net, (uint32_t)ck.level);
                if (clocks_.empty()) primary_ = 0;
                clocks_.push_back(ck);
                sim_.eval_combinational();
                tr_.record(sim_.time_ns(), sim_);
                break;
            }
            case TbCmd::Type::RESET: {
                uint16_t net = 0;
                if (!resolve_one(c.name, net)) suggest(c.name, r, c.line);
                break;  // polarity decl only; user drives it
            }
            case TbCmd::Type::DRIVE:
                write_value(c.name, c.value, r, c.line);
                break;
            case TbCmd::Type::WAIT_NS:
                advance_to(sim_.time_ns() + c.value, r);
                break;
            case TbCmd::Type::WAIT_EDGE: {
                uint16_t net = 0;
                if (!resolve_one(c.name, net)) { suggest(c.name, r, c.line); break; }
                // advance toggle-by-toggle until the edge hits (cap 1M toggles)
                bool hit = false;
                for (int i = 0; i < 1000000; ++i) {
                    uint64_t t_next = (uint64_t)-1;
                    for (size_t j = 0; j < clocks_.size(); ++j)
                        if (clocks_[j].net == net && clocks_[j].next_toggle < t_next)
                            t_next = clocks_[j].next_toggle;
                    if (t_next == (uint64_t)-1) break;  // not a clock: single eval
                    int before = sim_.read(net) ? 1 : 0;
                    advance_to(t_next, r);
                    int after = sim_.read(net) ? 1 : 0;
                    if (c.rising && !before && after) { hit = true; break; }
                    if (!c.rising && before && !after) { hit = true; break; }
                }
                if (!hit) {
                    char buf[128];
                    snprintf(buf, sizeof(buf),
                             "error line %lu: edge never occurred on '%s'",
                             (unsigned long)c.line, c.name.c_str());
                    r.messages.push_back(buf);
                    r.failed++;
                }
                break;
            }
            case TbCmd::Type::REPEAT:
                for (uint64_t i = 0; i < c.value && !r.stopped; ++i)
                    exec_list(c.block, r);
                break;
            case TbCmd::Type::ASSERT: {
                bool is_bus = false;
                uint16_t net = 0;
                std::vector<uint16_t> bits;
                if (!resolve_one(c.name, net) && !bus_nets(c.name, bits)) {
                    suggest(c.name, r, c.line);
                    break;
                }
                uint32_t actual = read_value(c.name, is_bus);
                if (check_op(c.op, actual, c.value)) {
                    r.passed++;
                } else {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "[FAIL] Time: %lu ns\nSignal: %s\nExpected: %s %lu\nActual:   %u",
                             (unsigned long)sim_.time_ns(), c.name.c_str(),
                             c.op.c_str(), (unsigned long)c.value, actual);
                    r.messages.push_back(buf);
                    r.failed++;
                }
                break;
            }
            case TbCmd::Type::TRACE: {
                uint16_t net = 0;
                std::vector<uint16_t> bits;
                if (bus_nets(c.name, bits)) {
                    for (size_t i = 0; i < bits.size(); ++i) tr_.watch(bits[i]);
                } else if (resolve_one(c.name, net)) {
                    tr_.watch(net);
                } else {
                    suggest(c.name, r, c.line);
                    break;
                }
                tr_.record(sim_.time_ns(), sim_);
                break;
            }
            case TbCmd::Type::STOP:
                r.stopped = true;
                break;
        }
    }
}

ExecResult TbExecutor::run(const Testbench &tb) {
    ExecResult r;
    sim_.reset();
    clocks_.clear();
    primary_ = 0;
    exec_list(tb.cmds, r);
    return r;
}

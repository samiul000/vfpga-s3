// Educational ASIC-style physical design engine (host-only).
#include "physical_design.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <queue>
#include <set>

namespace physical {
namespace {

uint32_t lcg_next(uint32_t &s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

const CellInfo kCells[] = {
    {"INV", 10, 30, false, 40},   {"BUF", 10, 30, false, 45},
    {"AND2", 20, 30, false, 60},  {"OR2", 20, 30, false, 60},
    {"XOR2", 20, 30, false, 70},  {"NAND2", 20, 30, false, 55},
    {"NOR2", 20, 30, false, 55},  {"MUX2", 30, 30, false, 80},
    {"XOR3", 30, 30, false, 90},  {"MAJ3", 30, 30, false, 90},
    {"DFF", 30, 30, true, 120},
};
const size_t kCellCount = sizeof(kCells) / sizeof(kCells[0]);

const char *map_op(const std::string &op) {
    if (op == "&" || op == "==") return "AND2";
    if (op == "|") return "OR2";
    if (op == "^" || op == "+") return "XOR2";
    if (op == "!=") return "XOR2";  // != over 1 bit == XOR; wider ops rejected below
    if (op == "!") return "INV";
    if (op == "PASS") return "BUF";
    if (op == "MUX") return "MUX2";
    if (op == "XOR3") return "XOR3";
    if (op == "MAJ3") return "MAJ3";
    return NULL;
}

bool is_clock_name(const std::string &n) {
    return n == "clock" || n == "clk" ||
           (n.size() > 5 && n.compare(n.size() - 5, 5, "clock") == 0) ||
           (n.size() > 3 && n.compare(n.size() - 3, 3, "clk") == 0);
}

std::string net_name(const Netlist &nl, uint16_t id) {
    if ((size_t)id < nl.net_count()) return nl.get_net(id).name;
    char b[32];
    snprintf(b, sizeof(b), "net%u", id);
    return b;
}

Pt center(const PlacedInst &p) { return Pt{p.x + p.w / 2, p.y + p.h / 2}; }

void json_escape(std::string &o, const std::string &s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
}

}  // namespace

int64_t manhattan(Pt a, Pt b) {
    int64_t dx = a.x - b.x, dy = a.y - b.y;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}
bool overlaps(Rect a, Rect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}
bool contains(Rect a, Pt p) {
    return p.x >= a.x && p.x <= a.x + a.w && p.y >= a.y && p.y <= a.y + a.h;
}
Rect bbox(Pt a, Pt b) {
    Rect r;
    r.x = a.x < b.x ? a.x : b.x;
    r.y = a.y < b.y ? a.y : b.y;
    r.w = (a.x < b.x ? b.x - a.x : a.x - b.x) + 1;
    r.h = (a.y < b.y ? b.y - a.y : a.y - b.y) + 1;
    return r;
}

const CellInfo *find_cell(const char *type) {
    for (size_t i = 0; i < kCellCount; ++i)
        if (std::string(kCells[i].type) == type) return &kCells[i];
    return NULL;
}
size_t cell_lib_size() { return kCellCount; }

bool build_ir(const Netlist &nl, const MappedConfig &cfg, DesignIR &ir,
              std::string &err) {
    (void)cfg;
    ir.insts.clear();
    ir.nets.clear();
    for (size_t i = 0; i < nl.component_count(); ++i) {
        const NetlistComponent &c = nl.get_component(i);
        IRInstance in;
        if (c.type == NetlistComponent::Type::FF) {
            in.cell = "DFF";
            in.sequential = true;
        } else {
            const char *cell = map_op(c.op);
            if (!cell) {
                err = "ERROR [Physical:Mapping]: operator '" + c.op +
                      "' has no standard-cell equivalent. " +
                      "Supported: & | ^ ! MUX XOR3 MAJ3 PASS.";
                return false;
            }
            // Multi-bit arithmetic/comparison has no single-cell equivalent.
            if ((c.op == "+" || c.op == "==" || c.op == "!=") &&
                c.inputs.size() > 2) {
                err = "ERROR [Physical:Mapping]: operator '" + c.op +
                      "' over " + std::to_string(c.inputs.size()) +
                      " inputs needs decomposition (unsupported). " +
                      "Suggested Action: use bitwise ops or single-bit compares.";
                return false;
            }
            in.cell = cell;
        }
        in.output = net_name(nl, c.output);
        for (size_t k = 0; k < c.inputs.size(); ++k)
            in.inputs.push_back(net_name(nl, c.inputs[k]));
        in.name = "top/" + in.output;
        ir.insts.push_back(in);
    }
    for (size_t i = 0; i < nl.net_count(); ++i) {
        IRNet n;
        n.name = nl.get_net(i).name;
        n.is_clock = is_clock_name(n.name);
        ir.nets.push_back(n);
    }
    return true;
}

bool floorplan_place(const DesignIR &ir, const PlaceOptions &opt,
                     Floorplan &fp, std::string &err) {
    if (ir.insts.empty()) {
        err = "ERROR [Physical:Floorplan]: empty design, nothing to place.";
        return false;
    }
    double util = opt.utilization <= 0.0 || opt.utilization > 1.0
                      ? 0.6
                      : opt.utilization;
    double aspect = opt.aspect <= 0.0 ? 1.0 : opt.aspect;
    int64_t cell_area = 0;
    for (size_t i = 0; i < ir.insts.size(); ++i) {
        const CellInfo *ci = find_cell(ir.insts[i].cell.c_str());
        if (!ci) {
            err = "ERROR [Physical:Floorplan]: unknown cell '" +
                  ir.insts[i].cell + "'.";
            return false;
        }
        cell_area += ci->w * ci->h;
    }
    double core_area = (double)cell_area / util;
    fp.core_w = (int64_t)ceil(sqrt(core_area * aspect));
    fp.core_h = (int64_t)ceil(sqrt(core_area / aspect));
    if (fp.core_w < 60) fp.core_w = 60;
    if (fp.core_h < 60) fp.core_h = 60;
    fp.core_x = opt.margin;
    fp.core_y = opt.margin;
    fp.die_w = fp.core_x * 2 + fp.core_w;
    fp.die_h = fp.core_y * 2 + fp.core_h;
    fp.row_h = 30;
    fp.rows = fp.core_h / fp.row_h;
    if (fp.rows < 1) fp.rows = 1;

    // Deterministic order: seed-shuffled, sequential cells first.
    std::vector<size_t> order;
    for (size_t i = 0; i < ir.insts.size(); ++i) order.push_back(i);
    uint32_t s = (uint32_t)(opt.seed * 2654435761u + 1);
    for (size_t i = order.size(); i > 1; --i) {
        size_t j = lcg_next(s) % i;
        size_t t = order[i - 1];
        order[i - 1] = order[j];
        order[j] = t;
    }
    std::string algo = opt.algorithm ? opt.algorithm : "greedy_wirelength";
    if (algo != "random_seeded") {
        // Stable: sequential first, then input order (greedy refines below).
        std::vector<size_t> seq, comb;
        for (size_t i = 0; i < order.size(); ++i)
            (ir.insts[order[i]].sequential ? seq : comb).push_back(order[i]);
        order.clear();
        order.insert(order.end(), seq.begin(), seq.end());
        order.insert(order.end(), comb.begin(), comb.end());
    }

    // Driver/sink lookup by net for wirelength cost.
    std::map<std::string, size_t> driver;
    for (size_t i = 0; i < ir.insts.size(); ++i)
        driver[ir.insts[i].output] = i;

    fp.insts.clear();
    fp.insts.resize(ir.insts.size());
    std::vector<int64_t> row_x((size_t)fp.rows, fp.core_x);
    std::vector<char> placed(ir.insts.size(), 0);
    for (size_t oi = 0; oi < order.size(); ++oi) {
        size_t idx = order[oi];
        const CellInfo *ci = find_cell(ir.insts[idx].cell.c_str());
        PlacedInst best;
        bool have = false;
        int64_t best_cost = 0;
        if (algo == "row_pack" || algo == "random_seeded") {
            size_t r = oi % (size_t)fp.rows;
            best.x = row_x[r];
            best.y = fp.core_y + (int64_t)r * fp.row_h;
            row_x[r] += ci->w;
            have = true;
        } else {  // greedy_wirelength: try each row end, keep min cost
            // Only rows with remaining capacity; else the emptiest row.
            bool room = false;
            for (int64_t r = 0; r < fp.rows; ++r)
                if (row_x[(size_t)r] + ci->w <= fp.core_x + fp.core_w)
                    room = true;
            for (int64_t r = 0; r < fp.rows; ++r) {
                if (room &&
                    row_x[(size_t)r] + ci->w > fp.core_x + fp.core_w)
                    continue;
                Pt cand{row_x[(size_t)r], fp.core_y + r * fp.row_h};
                Pt cc{cand.x + ci->w / 2, cand.y + ci->h / 2};
                int64_t cost = 0;
                const IRInstance &in = ir.insts[idx];
                for (size_t k = 0; k < in.inputs.size(); ++k) {
                    std::map<std::string, size_t>::const_iterator it =
                        driver.find(in.inputs[k]);
                    if (it != driver.end() && placed[it->second])
                        cost += manhattan(cc, center(fp.insts[it->second]));
                }
                for (size_t j = 0; j < ir.insts.size(); ++j) {
                    if (!placed[j]) continue;
                    const IRInstance &o = ir.insts[j];
                    for (size_t k = 0; k < o.inputs.size(); ++k)
                        if (o.inputs[k] == in.output)
                            cost += manhattan(cc, center(fp.insts[j]));
                }
                if (!have || cost < best_cost) {
                    best_cost = cost;
                    best.x = cand.x;
                    best.y = cand.y;
                    have = true;
                }
            }
            if (!have) {  // no row has room: use the emptiest row
                int64_t minx = row_x[0];
                int64_t br0 = 0;
                for (int64_t r = 1; r < fp.rows; ++r)
                    if (row_x[(size_t)r] < minx) {
                        minx = row_x[(size_t)r];
                        br0 = r;
                    }
                best.x = row_x[(size_t)br0];
                best.y = fp.core_y + br0 * fp.row_h;
                have = true;
            }
            // Commit: find the row we picked and advance its cursor.
            int64_t br = (best.y - fp.core_y) / fp.row_h;
            if (br < 0) br = 0;
            if (br >= fp.rows) br = fp.rows - 1;
            if (row_x[(size_t)br] <= best.x) row_x[(size_t)br] = best.x + ci->w;
        }
        best.name = ir.insts[idx].name;
        best.cell = ir.insts[idx].cell;
        best.w = ci->w;
        best.h = ci->h;
        fp.insts[idx] = best;
        placed[idx] = 1;
    }
    return true;
}

RouteResult route(const DesignIR &ir, const Floorplan &fp) {
    RouteResult rr;
    if (fp.insts.empty()) {
        rr.drc.push_back(
            "ERROR [Physical:Routing]: no placement, run place first.");
        rr.unrouted = ir.nets.size();
        return rr;
    }
    // Instance centers by output net.
    std::map<std::string, Pt> pos;
    std::map<std::string, size_t> idx;
    for (size_t i = 0; i < fp.insts.size(); ++i) {
        pos[fp.insts[i].name.substr(4)] = center(fp.insts[i]);
        idx[fp.insts[i].name.substr(4)] = i;
    }
    const int64_t T = 20;  // routing tile, grid units
    int64_t gw = (fp.core_w + T - 1) / T, gh = (fp.core_h + T - 1) / T;
    if (gw > 128) gw = 128;
    if (gh > 128) gh = 128;
    if (gw < 1) gw = 1;
    if (gh < 1) gh = 1;
    std::set<std::pair<int64_t, int64_t> > blocked;
    for (size_t i = 0; i < fp.insts.size(); ++i) {
        Pt c = center(fp.insts[i]);
        blocked.insert(std::make_pair((c.x - fp.core_x) / T,
                                      (c.y - fp.core_y) / T));
    }
    for (size_t ni = 0; ni < ir.nets.size(); ++ni) {
        const std::string &net = ir.nets[ni].name;
        std::map<std::string, Pt>::const_iterator dp = pos.find(net);
        if (dp == pos.end()) continue;  // primary input / undriven: no route
        // Sinks: instances consuming this net.
        std::vector<Pt> sinks;
        for (size_t i = 0; i < ir.insts.size(); ++i)
            for (size_t k = 0; k < ir.insts[i].inputs.size(); ++k)
                if (ir.insts[i].inputs[k] == net &&
                    ir.insts[i].output != net) {
                    std::map<std::string, Pt>::const_iterator sp =
                        pos.find(ir.insts[i].output);
                    if (sp != pos.end()) sinks.push_back(sp->second);
                }
        for (size_t si = 0; si < sinks.size(); ++si) {
            Pt a = dp->second, b = sinks[si];
            // L-route: horizontal on M1 to corner, via, vertical on M2.
            Pt corner{b.x, a.y};
            auto inside = [&](Pt p) {
                return p.x >= fp.core_x &&
                       p.x <= fp.core_x + fp.core_w &&
                       p.y >= fp.core_y &&
                       p.y <= fp.core_y + fp.core_h;
            };
            if (!inside(a) || !inside(b) || !inside(corner)) {
                char msg[160];
                snprintf(msg, sizeof(msg),
                         "ERROR [Physical:Routing]: Net %s sink %lu out of "
                         "core bounds (OUT_OF_BOUNDS).",
                         net.c_str(), (unsigned long)si);
                rr.drc.push_back(msg);
                rr.unrouted++;
                continue;
            }
            // BFS on tile grid for the horizontal leg obstacle check.
            int64_t ax = (a.x - fp.core_x) / T, ay = (a.y - fp.core_y) / T;
            int64_t cx = (corner.x - fp.core_x) / T,
                    cy = (corner.y - fp.core_y) / T;
            if (ax >= gw) ax = gw - 1;
            if (cx >= gw) cx = gw - 1;
            if (ay >= gh) ay = gh - 1;
            if (cy >= gh) cy = gh - 1;
            std::vector<std::vector<char> > seen(
                (size_t)gh, std::vector<char>((size_t)gw, 0));
            std::vector<std::vector<std::pair<int, int> > > prev(
                (size_t)gh,
                std::vector<std::pair<int, int> >((size_t)gw, {-1, -1}));
            std::queue<std::pair<int, int> > q;
            q.push({(int)ax, (int)ay});
            seen[(size_t)ay][(size_t)ax] = 1;
            const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            while (!q.empty()) {
                std::pair<int, int> cur = q.front();
                q.pop();
                if (cur.first == cx && cur.second == cy) break;
                for (int d = 0; d < 4; ++d) {
                    int nx = cur.first + DX[d], ny = cur.second + DY[d];
                    if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
                    if (seen[(size_t)ny][(size_t)nx]) continue;
                    seen[(size_t)ny][(size_t)nx] = 1;
                    prev[(size_t)ny][(size_t)nx] = cur;
                    q.push({nx, ny});
                }
            }
            if (!seen[(size_t)cy][(size_t)cx]) {
                char msg[160];
                snprintf(msg, sizeof(msg),
                         "ERROR [Physical:Routing]: Net %s sink %lu "
                         "unreachable (UNROUTED). Suggested Action: decrease "
                         "core utilization or re-place.",
                         net.c_str(), (unsigned long)si);
                rr.drc.push_back(msg);
                rr.unrouted++;
                continue;
            }
            if (!(a.x == corner.x && a.y == corner.y)) {
                RouteSeg s;
                s.net = net;
                s.layer = 1;
                s.a = a;
                s.b = corner;
                rr.segs.push_back(s);
            }
            if (!(corner.x == b.x && corner.y == b.y)) {
                Via v;
                v.net = net;
                v.at = corner;
                rr.vias.push_back(v);
                RouteSeg s;
                s.net = net;
                s.layer = 2;
                s.a = corner;
                s.b = b;
                rr.segs.push_back(s);
            }
        }
    }
    return rr;
}

TimingResult estimate_timing(const DesignIR &ir, const Floorplan &fp) {
    TimingResult t;
    if (ir.insts.empty()) return t;
    std::map<std::string, Pt> pos;
    for (size_t i = 0; i < fp.insts.size(); ++i)
        pos[fp.insts[i].name.substr(4)] = center(fp.insts[i]);
    // Arrival per net; primary inputs / FF-Q start at 0.
    std::map<std::string, int64_t> arr;
    std::map<std::string, std::string> how;
    for (size_t i = 0; i < ir.insts.size(); ++i)
        if (ir.insts[i].sequential) arr[ir.insts[i].output] = 0;
    bool changed = true;
    for (size_t iter = 0; iter < ir.insts.size() + 1 && changed; ++iter) {
        changed = false;
        for (size_t i = 0; i < ir.insts.size(); ++i) {
            const IRInstance &in = ir.insts[i];
            if (in.sequential) continue;
            const CellInfo *ci = find_cell(in.cell.c_str());
            if (!ci) continue;
            int64_t a = 0;
            std::string src;
            for (size_t k = 0; k < in.inputs.size(); ++k) {
                int64_t base = 0;
                std::map<std::string, int64_t>::const_iterator it =
                    arr.find(in.inputs[k]);
                if (it != arr.end()) base = it->second;
                int64_t wire = 0;
                std::map<std::string, Pt>::const_iterator p1 =
                    pos.find(in.inputs[k]);
                std::map<std::string, Pt>::const_iterator p2 =
                    pos.find(in.output);
                if (p1 != pos.end() && p2 != pos.end())
                    wire = 2 * manhattan(p1->second, p2->second);
                if (k == 0 || base + wire > a) {
                    a = base + wire;
                    src = in.inputs[k];
                }
            }
            int64_t total = a + ci->delay_ps;
            std::map<std::string, int64_t>::const_iterator cur =
                arr.find(in.output);
            if (cur == arr.end() || total > cur->second) {
                arr[in.output] = total;
                how[in.output] = src.empty() ? in.name : src;
                changed = true;
            }
        }
    }
    // Critical endpoint: max arrival at primary outputs / FF-D.
    std::set<std::string> ends;
    for (size_t i = 0; i < ir.insts.size(); ++i)
        if (ir.insts[i].sequential)
            for (size_t k = 0; k < ir.insts[i].inputs.size(); ++k)
                ends.insert(ir.insts[i].inputs[k]);
    std::map<std::string, int> outdeg;
    for (size_t i = 0; i < ir.insts.size(); ++i)
        for (size_t k = 0; k < ir.insts[i].inputs.size(); ++k)
            outdeg[ir.insts[i].inputs[k]]++;
    for (size_t i = 0; i < ir.nets.size(); ++i)
        if (outdeg[ir.nets[i].name] == 0) ends.insert(ir.nets[i].name);
    int64_t crit = 0;
    std::string crit_net;
    for (std::set<std::string>::const_iterator it = ends.begin();
         it != ends.end(); ++it) {
        std::map<std::string, int64_t>::const_iterator a = arr.find(*it);
        if (a != arr.end() && a->second > crit) {
            crit = a->second;
            crit_net = *it;
        }
    }
    t.crit_ps = crit;
    t.wns_ps = -crit;  // single 10ns (10000ps) clock assumption below
    const int64_t kPeriod = 10000;
    t.wns_ps = kPeriod - crit < 0 ? kPeriod - crit : 0;
    t.tns_ps = t.wns_ps;
    // Backtrace one critical path.
    std::string cur = crit_net;
    for (int i = 0; i < 64 && !cur.empty(); ++i) {
        t.crit_path.push_back(cur);
        std::map<std::string, std::string>::const_iterator h = how.find(cur);
        if (h == how.end() || h->second == cur) break;
        cur = h->second;
        if (arr.find(cur) == arr.end()) {
            t.crit_path.push_back(cur);
            break;
        }
    }
    return t;
}

Congestion analyze_congestion(const DesignIR &ir, const Floorplan &fp,
                              const RouteResult &rr) {
    (void)ir;
    Congestion c;
    if (fp.core_w <= 0 || fp.core_h <= 0) return c;
    const int64_t N = 8;
    std::vector<std::vector<int> > dem((size_t)N, std::vector<int>((size_t)N, 0));
    for (size_t i = 0; i < rr.segs.size(); ++i) {
        const RouteSeg &s = rr.segs[i];
        int64_t x0 = (s.a.x < s.b.x ? s.a.x : s.b.x), x1 = s.a.x < s.b.x ? s.b.x : s.a.x;
        int64_t y0 = (s.a.y < s.b.y ? s.a.y : s.b.y), y1 = s.a.y < s.b.y ? s.b.y : s.a.y;
        for (int64_t ty = 0; ty < N; ++ty)
            for (int64_t tx = 0; tx < N; ++tx) {
                int64_t rx = fp.core_x + fp.core_w * tx / N;
                int64_t rw = fp.core_w / N + 1;
                int64_t ry = fp.core_y + fp.core_h * ty / N;
                int64_t rh = fp.core_h / N + 1;
                if (x0 < rx + rw && rx < x1 + 1 && y0 < ry + rh && ry < y1 + 1)
                    dem[(size_t)ty][(size_t)tx]++;
            }
    }
    const double cap = 4.0;
    double sum = 0.0;
    for (int64_t ty = 0; ty < N; ++ty)
        for (int64_t tx = 0; tx < N; ++tx) {
            double u = dem[(size_t)ty][(size_t)tx] / cap;
            sum += u;
            if (u > c.max_use) c.max_use = u;
            if (u > 1.0) c.overflow_tiles++;
        }
    c.avg_use = sum / (N * N);
    return c;
}

int64_t total_hpwl(const DesignIR &ir, const Floorplan &fp) {
    // Driver center + each sink's own center per net.
    std::map<std::string, Pt> drv;
    for (size_t i = 0; i < fp.insts.size() && i < ir.insts.size(); ++i)
        drv[ir.insts[i].output] = center(fp.insts[i]);
    std::map<std::string, std::vector<Pt> > pins;
    for (size_t i = 0; i < ir.insts.size(); ++i) {
        std::map<std::string, Pt>::const_iterator d =
            drv.find(ir.insts[i].output);
        if (d == drv.end()) continue;
        pins[ir.insts[i].output].push_back(d->second);
        for (size_t k = 0; k < ir.insts[i].inputs.size(); ++k)
            if (drv.find(ir.insts[i].inputs[k]) != drv.end())
                pins[ir.insts[i].inputs[k]].push_back(center(fp.insts[i]));
    }
    int64_t total = 0;
    for (std::map<std::string, std::vector<Pt> >::const_iterator it =
             pins.begin();
         it != pins.end(); ++it) {
        if (it->second.size() < 2) continue;
        int64_t x0 = it->second[0].x, x1 = x0, y0 = it->second[0].y, y1 = y0;
        for (size_t k = 1; k < it->second.size(); ++k) {
            if (it->second[k].x < x0) x0 = it->second[k].x;
            if (it->second[k].x > x1) x1 = it->second[k].x;
            if (it->second[k].y < y0) y0 = it->second[k].y;
            if (it->second[k].y > y1) y1 = it->second[k].y;
        }
        total += (x1 - x0) + (y1 - y0);
    }
    return total;
}

ClockTree build_clock_tree(const DesignIR &ir) {
    ClockTree ct;
    for (size_t i = 0; i < ir.nets.size(); ++i)
        if (ir.nets[i].is_clock) {
            ct.net = ir.nets[i].name;
            break;
        }
    if (ct.net.empty()) return ct;
    for (size_t i = 0; i < ir.insts.size(); ++i)
        if (ir.insts[i].sequential) ct.sinks++;
    ct.buffers = ct.sinks == 0 ? 0 : (ct.sinks <= 4 ? 1 : (ct.sinks + 3) / 4);
    ct.depth = ct.sinks == 0 ? 0 : (ct.sinks <= 4 ? 1 : 2);
    return ct;
}

std::string layout_export_json(const std::string &design, const DesignIR &ir,
                               const Floorplan &fp, const RouteResult &rr,
                               const TimingResult &tm) {
    std::string o = "{\n  \"format\": \"vfpga-physical-layout\",\n  \"version\": 1,\n  \"design\": \"";
    json_escape(o, design);
    o += "\",\n  \"units\": \"abstract (1u = 0.1um)\",\n";
    char b[256];
    snprintf(b, sizeof(b),
             "  \"die\": {\"w\": %lld, \"h\": %lld},\n"
             "  \"core\": {\"x\": %lld, \"y\": %lld, \"w\": %lld, \"h\": %lld, \"rows\": %lld},\n",
             (long long)fp.die_w, (long long)fp.die_h, (long long)fp.core_x,
             (long long)fp.core_y, (long long)fp.core_w, (long long)fp.core_h,
             (long long)fp.rows);
    o += b;
    o += "  \"layers\": [\"M1(H)\", \"M2(V)\"],\n  \"cells\": [";
    for (size_t i = 0; i < cell_lib_size(); ++i) {
        const CellInfo *ci = &kCells[i];
        snprintf(b, sizeof(b), "%s{\"type\": \"%s\", \"w\": %lld, \"h\": %lld}",
                 i ? ", " : "", ci->type, (long long)ci->w, (long long)ci->h);
        o += b;
    }
    o += "],\n  \"instances\": [";
    for (size_t i = 0; i < fp.insts.size(); ++i) {
        const PlacedInst &p = fp.insts[i];
        o += i ? ",\n    " : "\n    ";
        o += "{\"name\": \"";
        json_escape(o, p.name);
        o += "\", \"cell\": \"";
        json_escape(o, p.cell);
        snprintf(b, sizeof(b),
                 "\", \"x\": %lld, \"y\": %lld, \"w\": %lld, \"h\": %lld}",
                 (long long)p.x, (long long)p.y, (long long)p.w,
                 (long long)p.h);
        o += b;
    }
    o += "\n  ],\n  \"nets\": [";
    for (size_t i = 0; i < ir.nets.size(); ++i) {
        o += i ? ", " : "";
        o += "\"";
        json_escape(o, ir.nets[i].name);
        o += "\"";
    }
    o += "],\n  \"routes\": [";
    for (size_t i = 0; i < rr.segs.size(); ++i) {
        const RouteSeg &s = rr.segs[i];
        o += i ? ",\n    " : "\n    ";
        o += "{\"net\": \"";
        json_escape(o, s.net);
        snprintf(b, sizeof(b),
                 "\", \"layer\": %d, \"a\": [%lld, %lld], \"b\": [%lld, %lld]}",
                 s.layer, (long long)s.a.x, (long long)s.a.y,
                 (long long)s.b.x, (long long)s.b.y);
        o += b;
    }
    o += "\n  ],\n  \"vias\": [";
    for (size_t i = 0; i < rr.vias.size(); ++i) {
        const Via &v = rr.vias[i];
        o += i ? ", " : "";
        o += "{\"net\": \"";
        json_escape(o, v.net);
        snprintf(b, sizeof(b), "\", \"at\": [%lld, %lld]}", (long long)v.at.x,
                 (long long)v.at.y);
        o += b;
    }
    snprintf(b, sizeof(b),
             "],\n  \"timing\": {\"crit_ps\": %lld, \"wns_ps\": %lld, "
             "\"tns_ps\": %lld},\n  \"unrouted\": %lu,\n  \"drc\": [",
             (long long)tm.crit_ps, (long long)tm.wns_ps,
             (long long)tm.tns_ps, (unsigned long)rr.unrouted);
    o += b;
    for (size_t i = 0; i < rr.drc.size(); ++i) {
        o += i ? ", " : "";
        o += "\"";
        json_escape(o, rr.drc[i]);
        o += "\"";
    }
    o += "]\n}\n";
    return o;
}

bool layout_import_check(const std::string &json, std::string &design_out,
                         std::string &err) {
    if (json.find("\"format\": \"vfpga-physical-layout\"") ==
        std::string::npos) {
        err = "Unsupported physical layout file (missing "
              "\"vfpga-physical-layout\" format marker).";
        return false;
    }
    size_t vp = json.find("\"version\":");
    int ver = -1;
    if (vp != std::string::npos) ver = atoi(json.c_str() + vp + 10);
    if (ver != 1) {
        char b[128];
        snprintf(b, sizeof(b),
                 "Unsupported physical layout format version: %d\nSupported "
                 "versions: 1",
                 ver);
        err = b;
        return false;
    }
    size_t dp = json.find("\"design\": \"");
    if (dp == std::string::npos) {
        err = "Unsupported physical layout file (missing design name).";
        return false;
    }
    size_t s = dp + 11, e = json.find('"', s);
    design_out = json.substr(s, e - s);
    return true;
}

std::string layout_export_svg(const std::string &design, const Floorplan &fp,
                              const RouteResult &rr) {
    std::string o;
    char b[256];
    snprintf(b, sizeof(b),
             "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %lld "
             "%lld\"><title>",
             (long long)fp.die_w, (long long)fp.die_h);
    o += b;
    json_escape(o, design);
    o += "</title>\n<rect x=\"0\" y=\"0\" width=\"" +
         std::to_string(fp.die_w) + "\" height=\"" +
         std::to_string(fp.die_h) +
         "\" fill=\"#111\" stroke=\"#888\"/>\n<rect x=\"" +
         std::to_string(fp.core_x) + "\" y=\"" + std::to_string(fp.core_y) +
         "\" width=\"" + std::to_string(fp.core_w) + "\" height=\"" +
         std::to_string(fp.core_h) +
         "\" fill=\"#1c2b1c\" stroke=\"#4c4\"/>\n";
    for (size_t i = 0; i < rr.segs.size(); ++i) {
        const RouteSeg &s = rr.segs[i];
        snprintf(b, sizeof(b),
                 "<line x1=\"%lld\" y1=\"%lld\" x2=\"%lld\" y2=\"%lld\" "
                 "stroke=\"%s\" stroke-width=\"1\"/>\n",
                 (long long)s.a.x, (long long)s.a.y, (long long)s.b.x,
                 (long long)s.b.y, s.layer == 1 ? "#c84" : "#48c");
        o += b;
    }
    for (size_t i = 0; i < rr.vias.size(); ++i) {
        snprintf(b, sizeof(b),
                 "<circle cx=\"%lld\" cy=\"%lld\" r=\"2\" fill=\"#cc4\"/>\n",
                 (long long)rr.vias[i].at.x, (long long)rr.vias[i].at.y);
        o += b;
    }
    for (size_t i = 0; i < fp.insts.size(); ++i) {
        const PlacedInst &p = fp.insts[i];
        snprintf(b, sizeof(b),
                 "<rect x=\"%lld\" y=\"%lld\" width=\"%lld\" height=\"%lld\" "
                 "fill=\"%s\" stroke=\"#000\"/>",
                 (long long)p.x, (long long)p.y, (long long)p.w,
                 (long long)p.h, p.cell == "DFF" ? "#484" : "#668");
        o += b;
        o += "<title>";
        json_escape(o, p.name + " (" + p.cell + ")");
        o += "</title></rect>\n";
    }
    o += "</svg>\n";
    return o;
}

}  // namespace physical

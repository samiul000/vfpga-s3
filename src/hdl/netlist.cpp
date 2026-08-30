#include "netlist.h"
#include <cstdio>

void Netlist::add_net(const std::string &name) {
    nets_.push_back(Net{static_cast<uint16_t>(nets_.size()), name});
}

void Netlist::add_component(NetlistComponent comp) {
    components_.push_back(comp);
}

size_t Netlist::net_count() const { return nets_.size(); }
size_t Netlist::component_count() const { return components_.size(); }
const Net &Netlist::get_net(size_t idx) const { return nets_[idx]; }
const NetlistComponent &Netlist::get_component(size_t idx) const { return components_[idx]; }
const std::vector<std::string> &Netlist::input_names() const { return input_names_; }
const std::vector<std::string> &Netlist::output_names() const { return output_names_; }
uint16_t Netlist::next_net_id() const { return static_cast<uint16_t>(nets_.size()); }

int16_t Netlist::resolve(const std::string &name) const {
    for (const auto &n : nets_) {
        if (n.name == name) return n.id;
    }
    return -1;
}

uint16_t Netlist::ensure_net(const std::string &name) {
    int16_t id = resolve(name);
    if (id >= 0) return static_cast<uint16_t>(id);
    add_net(name);
    return static_cast<uint16_t>(nets_.size() - 1);
}

uint16_t Netlist::ensure_bus_net(const std::string &name, int32_t bit) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s[%ld]", name.c_str(), (long)bit);
    return ensure_net(buf);
}

void Netlist::build_from_ast(const std::vector<AstNode> &ast) {
    std::string module_name;
    std::vector<std::pair<std::string, int32_t>> ports;
    std::vector<std::pair<std::string, int32_t>> wires;
    std::vector<std::pair<std::string, int32_t>> registers;

    for (const auto &node : ast) {
        switch (node.type) {
            case AstNode::Type::MODULE:
                module_name = node.name;
                break;
            case AstNode::Type::INPUT:
            case AstNode::Type::OUTPUT: {
                int32_t width = (node.msb >= 0) ? (node.msb - node.lsb + 1) : 1;
                ports.push_back({node.name, width});
                break;
            }
            case AstNode::Type::WIRE: {
                int32_t width = (node.msb >= 0) ? (node.msb - node.lsb + 1) : 1;
                wires.push_back({node.name, width});
                break;
            }
            case AstNode::Type::REGISTER: {
                int32_t width = (node.msb >= 0) ? (node.msb - node.lsb + 1) : 1;
                registers.push_back({node.name, width});
                break;
            }
            default: break;
        }
    }

    for (auto &[name, width] : ports) {
        ensure_net(name);
        for (int32_t i = 0; i < width; ++i) {
            ensure_bus_net(name, i);
        }
    }
    for (auto &[name, width] : wires) {
        ensure_net(name);
        for (int32_t i = 0; i < width; ++i) {
            ensure_bus_net(name, i);
        }
    }
    for (auto &[name, width] : registers) {
        ensure_net(name);
        for (int32_t i = 0; i < width; ++i) {
            ensure_bus_net(name, i);
        }
    }

    for (const auto &node : ast) {
        if (node.type == AstNode::Type::INPUT) input_names_.push_back(node.name);
        if (node.type == AstNode::Type::OUTPUT) output_names_.push_back(node.name);
    }

    for (const auto &node : ast) {
        if (node.type == AstNode::Type::ASSIGN) {
            uint16_t out_id = ensure_net(node.name);

            if (node.children.empty()) continue;

            NetlistComponent comp;
            comp.type = NetlistComponent::Type::LUT4;
            comp.id = static_cast<uint16_t>(components_.size());
            comp.op = node.op.empty() ? "PASS" : node.op;

            for (const auto &child : node.children) {
                int16_t child_id = resolve(child);
                if (child_id < 0) {
                    ensure_net(child);
                    child_id = resolve(child);
                }
                if (child_id >= 0) {
                    comp.inputs.push_back(static_cast<uint16_t>(child_id));
                }
            }
            comp.output = out_id;
            components_.push_back(comp);
        }

        if (node.type == AstNode::Type::ASSIGN_LE) {
            for (const auto &reg : registers) {
                if (resolve(reg.first) >= 0) {
                    uint16_t reg_id = ensure_net(reg.first);
                    uint16_t d_id = reg_id;
                    if (!node.children.empty()) {
                        int16_t cid = resolve(node.children[0]);
                        if (cid >= 0) d_id = static_cast<uint16_t>(cid);
                    }

                    NetlistComponent comp;
                    comp.type = NetlistComponent::Type::FF;
                    comp.id = static_cast<uint16_t>(components_.size());
                    comp.op = "DFF";
                    comp.inputs.push_back(d_id);
                    comp.output = reg_id;
                    components_.push_back(comp);
                    break;
                }
            }
        }
    }

    for (const auto &node : ast) {
        if (node.type == AstNode::Type::ALWAYS_POSEDGE) {
            // Traverse sub_nodes to find ASSIGN_LE and IF nodes
            // For each register assigned in the always block, wire its FF D-input
            for (const auto &sub : node.sub_nodes) {
                if (sub.type == AstNode::Type::ASSIGN_LE) {
                    // Direct assignment: reg <= expr
                    int16_t reg_id = resolve(sub.name);
                    if (reg_id < 0) continue;

                    // Create a LUT for the D-input expression
                    NetlistComponent lut_comp;
                    lut_comp.type = NetlistComponent::Type::LUT4;
                    lut_comp.id = static_cast<uint16_t>(components_.size());
                    lut_comp.op = sub.op.empty() ? "PASS" : sub.op;
                    lut_comp.output = static_cast<uint16_t>(reg_id);

                    for (const auto &child : sub.children) {
                        int16_t child_id = resolve(child);
                        if (child_id < 0) {
                            ensure_net(child);
                            child_id = resolve(child);
                        }
                        if (child_id >= 0) {
                            lut_comp.inputs.push_back(static_cast<uint16_t>(child_id));
                        }
                    }
                    if (lut_comp.inputs.empty()) {
                        // Constant value - create a const net
                        uint16_t const_id = ensure_net(sub.name + "_const");
                        lut_comp.inputs.push_back(const_id);
                    }
                    components_.push_back(lut_comp);

                    // Create FF: D = lut output, Q = reg
                    NetlistComponent ff_comp;
                    ff_comp.type = NetlistComponent::Type::FF;
                    ff_comp.id = static_cast<uint16_t>(components_.size());
                    ff_comp.op = "DFF";
                    ff_comp.inputs.push_back(static_cast<uint16_t>(reg_id));
                    ff_comp.output = static_cast<uint16_t>(reg_id);
                    components_.push_back(ff_comp);

                } else if (sub.type == AstNode::Type::IF) {
                    // if/else: create condition LUT + branch LUTs + MUX
                    // For simplicity, handle the common pattern:
                    // if (cond) begin reg <= val_a; end else begin reg <= val_b; end
                    // Create: cond_LUT, val_a_LUT, val_b_LUT, MUX_LUT, FF

                    // Condition LUT (NOT if op is "!")
                    uint16_t cond_out = ensure_net("_cond_" + std::to_string(components_.size()));
                    NetlistComponent cond_comp;
                    cond_comp.type = NetlistComponent::Type::LUT4;
                    cond_comp.id = static_cast<uint16_t>(components_.size());
                    cond_comp.op = sub.op.empty() ? "PASS" : sub.op;
                    cond_comp.output = cond_out;
                    for (const auto &child : sub.children) {
                        int16_t child_id = resolve(child);
                        if (child_id < 0) { ensure_net(child); child_id = resolve(child); }
                        if (child_id >= 0) cond_comp.inputs.push_back(static_cast<uint16_t>(child_id));
                    }
                    components_.push_back(cond_comp);

                    // Process then/else blocks
                    if (sub.sub_nodes.size() >= 1) {
                        const AstNode &then_block = sub.sub_nodes[0];
                        // Find assignments in then block (could be BEGIN_END or direct ASSIGN_LE)
                        std::vector<const AstNode*> then_assigns;
                        if (then_block.type == AstNode::Type::BEGIN_END) {
                            for (const auto &s : then_block.sub_nodes)
                                if (s.type == AstNode::Type::ASSIGN_LE) then_assigns.push_back(&s);
                        } else if (then_block.type == AstNode::Type::ASSIGN_LE) {
                            then_assigns.push_back(&then_block);
                        }

                        std::vector<const AstNode*> else_assigns;
                        if (sub.sub_nodes.size() >= 2) {
                            const AstNode &else_block = sub.sub_nodes[1];
                            if (else_block.type == AstNode::Type::BEGIN_END) {
                                for (const auto &s : else_block.sub_nodes)
                                    if (s.type == AstNode::Type::ASSIGN_LE) else_assigns.push_back(&s);
                            } else if (else_block.type == AstNode::Type::ASSIGN_LE) {
                                else_assigns.push_back(&else_block);
                            }
                        }

                        // For each assigned register, create branch LUTs + MUX + FF
                        for (size_t ai = 0; ai < then_assigns.size(); ++ai) {
                            const AstNode *then_a = then_assigns[ai];
                            int16_t reg_id = resolve(then_a->name);
                            if (reg_id < 0) continue;

                            // Then-value LUT
                            uint16_t then_out = ensure_net("_then_" + std::to_string(reg_id));
                            NetlistComponent then_comp;
                            then_comp.type = NetlistComponent::Type::LUT4;
                            then_comp.id = static_cast<uint16_t>(components_.size());
                            then_comp.op = then_a->op.empty() ? "PASS" : then_a->op;
                            then_comp.output = then_out;
                            for (const auto &child : then_a->children) {
                                int16_t child_id = resolve(child);
                                if (child_id < 0) { ensure_net(child); child_id = resolve(child); }
                                if (child_id >= 0) then_comp.inputs.push_back(static_cast<uint16_t>(child_id));
                            }
                            if (then_comp.inputs.empty()) {
                                uint16_t c = ensure_net(then_a->name + "_then_const");
                                then_comp.inputs.push_back(c);
                            }
                            components_.push_back(then_comp);

                            // Else-value LUT
                            uint16_t else_out = ensure_net("_else_" + std::to_string(reg_id));
                            NetlistComponent else_comp;
                            else_comp.type = NetlistComponent::Type::LUT4;
                            else_comp.id = static_cast<uint16_t>(components_.size());
                            if (ai < else_assigns.size()) {
                                const AstNode *else_a = else_assigns[ai];
                                else_comp.op = else_a->op.empty() ? "PASS" : else_a->op;
                                for (const auto &child : else_a->children) {
                                    int16_t child_id = resolve(child);
                                    if (child_id < 0) { ensure_net(child); child_id = resolve(child); }
                                    if (child_id >= 0) else_comp.inputs.push_back(static_cast<uint16_t>(child_id));
                                }
                                if (else_comp.inputs.empty()) {
                                    uint16_t c = ensure_net(else_a->name + "_else_const");
                                    else_comp.inputs.push_back(c);
                                }
                            } else {
                                else_comp.op = "PASS";
                                else_comp.inputs.push_back(static_cast<uint16_t>(reg_id));
                            }
                            else_comp.output = else_out;
                            components_.push_back(else_comp);

                            // MUX LUT: cond ? then_val : else_val
                            // Truth table 0x00CA: a(bit0)=else, b(bit1)=then, c(bit2)=cond
                            // c=0 -> a(else), c=1 -> b(then)
                            // 4th input (d) must be tied to GND to avoid clock leaking in
                            uint16_t mux_out = ensure_net("_mux_" + std::to_string(reg_id));
                            uint16_t gnd_net = ensure_net("GND");
                            NetlistComponent mux_comp;
                            mux_comp.type = NetlistComponent::Type::LUT4;
                            mux_comp.id = static_cast<uint16_t>(components_.size());
                            mux_comp.op = "MUX";
                            mux_comp.output = mux_out;
                            mux_comp.inputs.push_back(else_out);
                            mux_comp.inputs.push_back(then_out);
                            mux_comp.inputs.push_back(cond_out);
                            mux_comp.inputs.push_back(gnd_net);
                            components_.push_back(mux_comp);

                            // FF: D = mux_out, Q = reg
                            NetlistComponent ff_comp;
                            ff_comp.type = NetlistComponent::Type::FF;
                            ff_comp.id = static_cast<uint16_t>(components_.size());
                            ff_comp.op = "DFF";
                            ff_comp.inputs.push_back(mux_out);
                            ff_comp.output = static_cast<uint16_t>(reg_id);
                            components_.push_back(ff_comp);
                        }
                    }
                }
            }
        }
    }

    for (const auto &[name, width] : wires) {
        for (int32_t i = 0; i < width; ++i) {
            uint16_t wire_net = ensure_bus_net(name, i);
            if (resolve(name) >= 0) {
                NetlistComponent comp;
                comp.type = NetlistComponent::Type::LUT4;
                comp.id = static_cast<uint16_t>(components_.size());
                comp.op = "PASS";
                comp.inputs.push_back(ensure_bus_net(name, i));
                comp.output = wire_net;
                components_.push_back(comp);
            }
        }
    }
}

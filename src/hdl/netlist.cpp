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
                            // truth table: cond(bit0)=0 -> else, cond=1 -> then
                            // idx: val_a(bit1) | val_b(bit2) | cond(bit0)
                            // d=0: output=else; d=1: output=then -> truth table 0xA
                            // Actually: sel=cond, when cond=0 -> else, cond=1 -> then
                            // idx bit0=cond: cond=0 -> else (bit1=val_b), cond=1 -> then (bit2=val_a)
                            // bits: 2,3,6,7,10,11,14,15 for then; bits 0,1,4,5,8,9,12,13 for else
                            // = 0xF0F0 for cond as sel... let me use 0xCC for simplicity
                            // sel=bit0: 0->else, 1->then: truth table bits where output=1:
                            // cond=1: idx 1,3,5,7,9,11,13,15 -> then
                            // cond=0: idx 0,2,4,6,8,10,12,14 -> else
                            // MUX: cond=1->then(cond=bit0), then is on val_a(bit1), else is on val_b(bit2)
                            // For MUX(cond, then_val, else_val) with a=then_val, b=else_val, d=cond:
                            // cond=0->b(else), cond=1->a(then): truth table 0xAAAA (bit0=cond -> when 1, output=a)
                            // Wait: a(bit0), b(bit1), c(ignored), d(cond=bit3)
                            // Actually I need to be more careful. Let me use a simpler approach.
                            // MUX: inputs[0]=then_val, inputs[1]=else_val, inputs[2]=cond
                            // When cond=0: output=else_val; when cond=1: output=then_val
                            // With a=then_val(bit0), b=else_val(bit1), d=cond(bit2)
                            // idx: a|2b|4d
                            // d=0: idx 0(a=0,b=0)->0, 1(a=1,b=0)->1(then), 2(a=0,b=1)->0(else), 3(a=1,b=1)->1
                            // d=1: idx 4(a=0,b=0)->0, 5(a=1,b=0)->1(then), 6(a=0,b=1)->0(else), 7(a=1,b=1)->1
                            // Hmm that's just a. Let me use a different mapping.
                            // a=then_val, b=else_val, d=cond. Use d as bit3, a as bit0, b as bit1
                            // cond=0(d=0): output=else=b, cond=1(d=1): output=then=a
                            // d=0: idx 0->0, 1->1(a), 2->0(b=0), 3->1(a)
                            // d=1: idx 8->0, 9->0(a=0), 10->1(b=1), 11->1
                            // bits: 1,3,10,11 = 0x0C8A... this is getting complicated
                            // Let me just use the PASS truth table approach differently.
                            // Simplest: 3-input MUX with a=then, b=else, sel=cond
                            // I'll use truth table 0xF0F0: when cond=1->then, cond=0->else
                            // Actually let me just use: cond as bit0, then_val as bit1, else_val as bit2
                            // MUX: cond=0->else_val(bit2), cond=1->then_val(bit1)
                            // idx: cond(bit0)|then(bit1)|else(bit2)
                            // 0(0,0,0)->0, 1(1,0,0)->0, 2(0,1,0)->1(then), 3(1,1,0)->1(then)
                            // 4(0,0,1)->1(else), 5(1,0,1)->1(else), 6(0,1,1)->1, 7(1,1,1)->1
                            // bits 2,3,4,5,6,7 = 0xFC... no that's not right either
                            // OK let me just use: cond as bit2, then as bit0, else as bit1
                            // cond=0: output=else(bit1), cond=1: output=then(bit0)
                            // idx: then(bit0)|else(bit1)|cond(bit2)
                            // 0(0,0,0)->0, 1(1,0,0)->1(then), 2(0,1,0)->1(else), 3(1,1,0)->1
                            // 4(0,0,1)->0, 5(1,0,1)->1(then), 6(0,1,1)->0, 7(1,1,1)->1
                            // bits: 1,2,3,5,7 = 0xAE... still not right
                            // cond=0(d=0): want else; cond=1(d=1): want then
                            // d is bit2: d=0 -> idx 0-3, d=1 -> idx 4-7
                            // d=0: idx 0(000)->else=0, 1(001)->then=1, 2(010)->else=1, 3(011)->then=1
                            // d=1: idx 4(100)->then=0, 5(101)->then=1, 6(110)->else=0, 7(111)->then=1
                            // output=then when d=1 or (d=0 and then=1 and else=0)... this is wrong
                            // Let me just hardcode: MUX with sel=cond, a=then, b=else
                            // Using the same scheme as the test MUX but with 3 inputs:
                            // Actually, let me use a simpler scheme.
                            // cond(bit2), then_val(bit0), else_val(bit1)
                            // When cond=0: output = else_val (regardless of then_val)
                            // When cond=1: output = then_val (regardless of else_val)
                            // idx: then(bit0)|else(bit1)|cond(bit2)
                            // cond=0 (bit2=0): idx 0->0, 1->1(then), 2->1(else), 3->1
                            //   But we want: cond=0 -> else_val, so idx 0->0, 1->0, 2->1, 3->1
                            // cond=1 (bit2=1): idx 4->0, 5->1(then), 6->0, 7->1
                            //   We want: cond=1 -> then_val, so idx 4->0, 5->1, 6->0, 7->1
                            // bits: 2,3,5,7 = 0xAC
                            uint16_t mux_out = ensure_net("_mux_" + std::to_string(reg_id));
                            NetlistComponent mux_comp;
                            mux_comp.type = NetlistComponent::Type::LUT4;
                            mux_comp.id = static_cast<uint16_t>(components_.size());
                            mux_comp.op = "MUX";
                            mux_comp.output = mux_out;
                            mux_comp.inputs.push_back(then_out);
                            mux_comp.inputs.push_back(else_out);
                            mux_comp.inputs.push_back(cond_out);
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

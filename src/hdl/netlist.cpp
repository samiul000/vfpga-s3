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
            const std::string &src = node.children[0];
            int16_t src_id = resolve(src);
            if (src_id < 0) {
                ensure_net(src);
                src_id = resolve(src);
            }

            if (node.op.empty()) {
                NetlistComponent comp;
                comp.type = NetlistComponent::Type::LUT4;
                comp.id = static_cast<uint16_t>(components_.size());
                comp.op = "PASS";
                comp.inputs.push_back(static_cast<uint16_t>(src_id));
                comp.output = out_id;
                components_.push_back(comp);
            } else {
                NetlistComponent comp;
                comp.type = NetlistComponent::Type::LUT4;
                comp.id = static_cast<uint16_t>(components_.size());
                comp.op = node.op;
                comp.inputs.push_back(static_cast<uint16_t>(src_id));
                comp.output = out_id;
                components_.push_back(comp);
            }
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
            for (const auto &reg : registers) {
                uint16_t reg_id = ensure_net(reg.first);

                NetlistComponent comp;
                comp.type = NetlistComponent::Type::FF;
                comp.id = static_cast<uint16_t>(components_.size());
                comp.op = "DFF";
                comp.inputs.push_back(reg_id);
                comp.output = reg_id;
                components_.push_back(comp);
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

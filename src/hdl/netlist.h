#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "parser.h"

struct Net {
    uint16_t id;
    std::string name;
};

struct NetlistComponent {
    enum class Type { LUT4, FF };
    Type type;
    uint16_t id;
    std::string op;
    std::vector<uint16_t> inputs;
    uint16_t output;
};

class Netlist {
public:
    void add_net(const std::string &name);
    void add_component(NetlistComponent comp);
    void build_from_ast(const std::vector<AstNode> &ast);

    int16_t resolve(const std::string &name) const;
    size_t net_count() const;
    size_t component_count() const;
    const Net &get_net(size_t idx) const;
    const NetlistComponent &get_component(size_t idx) const;
    const std::vector<std::string> &input_names() const;
    const std::vector<std::string> &output_names() const;
    uint16_t next_net_id() const;

private:
    std::vector<Net> nets_;
    std::vector<NetlistComponent> components_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    uint16_t ensure_net(const std::string &name);
    uint16_t ensure_bus_net(const std::string &name, int32_t bit);
};

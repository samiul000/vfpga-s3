#pragma once

#include <cstdint>
#include <vector>

struct Net {
    uint16_t id;
    std::string name;
};

struct NetlistComponent {
    enum class Type { LUT4, FF, MUX };
    Type type;
    uint16_t id;
    std::vector<uint16_t> inputs;
    uint16_t output;
};

class Netlist {
public:
    void add_net(const std::string &name);
    void add_component(NetlistComponent comp);
    size_t net_count() const;
    size_t component_count() const;

private:
    std::vector<Net> nets_;
    std::vector<NetlistComponent> components_;
};

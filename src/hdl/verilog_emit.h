#pragma once
/*
   Behavioral Verilog emitter: AST -> synthesizable Verilog.
   Reads the parser AST only; never touches netlist/mapper/fabric state.
   Stdlib-only so it builds under both the Xtensa firmware toolchain and
   host g++ (CI). See docs/HDL_GUIDE.md "Verilog emission" for the
   supported subset and the known ADD (carry) divergence.
*/
#include <string>
#include <vector>
#include "parser.h"

class VerilogEmitter {
public:
    // Emit a complete Verilog module. port order follows AST declaration order.
    std::string emit(const std::string &module_name,
                     const std::vector<AstNode> &ast);

private:
    std::string range(const AstNode &n) const;
    std::string rhs(const AstNode &n) const;
    std::string cond(const AstNode &n) const;
    void stmt(const AstNode &n, std::string &out, int indent) const;
    static void pad(std::string &out, int indent);
};

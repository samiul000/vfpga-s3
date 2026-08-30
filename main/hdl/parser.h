#pragma once

#include <vector>
#include "lexer.h"

struct AstNode {
    enum class Type { MODULE, INPUT, OUTPUT, WIRE, ASSIGN, EXPR_AND, EXPR_OR, EXPR_XOR, EXPR_NOT, EXPR_IDENT, EXPR_NUMBER };
    Type type;
    std::string name;
    std::string expr_left;
    std::string expr_right;
};

class Parser {
public:
    std::vector<AstNode> parse(const std::vector<Token> &tokens);
};

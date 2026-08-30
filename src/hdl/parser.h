#pragma once

#include <vector>
#include <string>
#include "lexer.h"

struct AstNode {
    enum class Type {
        MODULE, INPUT, OUTPUT, WIRE, REGISTER,
        ASSIGN, ASSIGN_LE, ALWAYS_POSEDGE,
        IF, BEGIN_END,
        EXPR_BINARY, EXPR_NOT, EXPR_IDENT, EXPR_NUMBER, EXPR_HEX,
        BIT_SELECT, RANGE_SELECT, CONCAT,
        RANGE_DECL
    };

    Type type;
    std::string name;
    std::string op;
    std::vector<std::string> children;
    std::vector<int32_t> child_idx;
    std::vector<AstNode> sub_nodes;
    int32_t bit_index = -1;
    int32_t msb = -1, lsb = -1;
};

class Parser {
public:
    std::vector<AstNode> parse(const std::vector<Token> &tokens);

private:
    size_t pos_ = 0;
    const std::vector<Token> *tokens_ = nullptr;

    const Token &peek();
    const Token &advance();
    bool check(TokenType type);
    bool match(TokenType type);
    void expect(TokenType type, const char *msg);

    AstNode parse_module();
    AstNode parse_port_decl();
    AstNode parse_wire_decl();
    AstNode parse_register_decl();
    AstNode parse_assign();
    AstNode parse_always();
    AstNode parse_if();
    AstNode parse_begin_block();
    AstNode parse_assign_le();
    AstNode parse_expr();
    AstNode parse_unary_expr();
    AstNode parse_primary();
    int32_t parse_number();
};

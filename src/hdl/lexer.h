#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

enum class TokenType {
    IDENT, NUMBER, HEX_NUMBER,
    KW_MODULE, KW_INPUT, KW_OUTPUT, KW_WIRE, KW_ASSIGN, KW_REGISTER, KW_CLOCK,
    KW_ALWAYS, KW_POSEDGE, KW_BEGIN, KW_END, KW_ENDMODULE, KW_ELSE, KW_IF,
    OP_AND, OP_OR, OP_XOR, OP_NOT, OP_PLUS, OP_DIV, OP_EQ, OP_NEQ,
    ASSIGN_LE, EQUALS, COLON,
    LPAREN, RPAREN, LBRACKET, RBRACKET, LBRACE, RBRACE, SEMICOLON, COMMA,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string text;
    size_t line;
};

class Lexer {
public:
    std::vector<Token> tokenize(const std::string &source);

private:
    Token next_token(const std::string &source, size_t &pos, size_t &line);
};

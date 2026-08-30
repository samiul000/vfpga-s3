#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

enum class TokenType { IDENT, KW_MODULE, KW_INPUT, KW_OUTPUT, KW_WIRE, KW_ASSIGN, KW_REGISTER, KW_CLOCK, OP_AND, OP_OR, OP_XOR, OP_NOT, LPAREN, RPAREN, SEMICOLON, COMMA, EQUALS, NUMBER, END_OF_FILE };

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

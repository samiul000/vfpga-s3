#include "lexer.h"

std::vector<Token> Lexer::tokenize(const std::string &source) {
    std::vector<Token> tokens;
    size_t pos = 0;
    size_t line = 1;
    while (pos < source.size()) {
        Token t = next_token(source, pos, line);
        tokens.push_back(t);
        if (t.type == TokenType::END_OF_FILE) break;
    }
    return tokens;
}

Token Lexer::next_token(const std::string &source, size_t &pos, size_t &line) {
    while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\n' || source[pos] == '\r')) {
        if (source[pos] == '\n') ++line;
        ++pos;
    }
    if (pos >= source.size()) return {TokenType::END_OF_FILE, "", line};

    char c = source[pos];
    if (c == '(') { ++pos; return {TokenType::LPAREN, "(", line}; }
    if (c == ')') { ++pos; return {TokenType::RPAREN, ")", line}; }
    if (c == ';') { ++pos; return {TokenType::SEMICOLON, ";", line}; }
    if (c == ',') { ++pos; return {TokenType::COMMA, ",", line}; }
    if (c == '=') { ++pos; return {TokenType::EQUALS, "=", line}; }
    if (c == '&') { ++pos; return {TokenType::OP_AND, "&", line}; }
    if (c == '|') { ++pos; return {TokenType::OP_OR, "|", line}; }
    if (c == '^') { ++pos; return {TokenType::OP_XOR, "^", line}; }
    if (c == '!') { ++pos; return {TokenType::OP_NOT, "!", line}; }

    if (isalpha(c) || c == '_') {
        std::string id;
        while (pos < source.size() && (isalnum(source[pos]) || source[pos] == '_')) id += source[pos++];
        if (id == "module") return {TokenType::KW_MODULE, id, line};
        if (id == "input") return {TokenType::KW_INPUT, id, line};
        if (id == "output") return {TokenType::KW_OUTPUT, id, line};
        if (id == "wire") return {TokenType::KW_WIRE, id, line};
        if (id == "assign") return {TokenType::KW_ASSIGN, id, line};
        if (id == "register") return {TokenType::KW_REGISTER, id, line};
        if (id == "clock") return {TokenType::KW_CLOCK, id, line};
        return {TokenType::IDENT, id, line};
    }

    ++pos;
    return {TokenType::END_OF_FILE, "", line};
}

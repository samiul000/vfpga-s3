#include "lexer.h"
#include <cstdio>

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
    while (pos < source.size()) {
        if (source[pos] == '\n') { ++line; ++pos; }
        else if (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\r') { ++pos; }
        else if (source[pos] == '/' && pos + 1 < source.size() && source[pos + 1] == '/') {
            while (pos < source.size() && source[pos] != '\n') ++pos;
        } else break;
    }
    if (pos >= source.size()) return {TokenType::END_OF_FILE, "", line};

    char c = source[pos];

    if (c == '(') { ++pos; return {TokenType::LPAREN, "(", line}; }
    if (c == ')') { ++pos; return {TokenType::RPAREN, ")", line}; }
    if (c == '[') { ++pos; return {TokenType::LBRACKET, "[", line}; }
    if (c == ']') { ++pos; return {TokenType::RBRACKET, "]", line}; }
    if (c == '{') { ++pos; return {TokenType::LBRACE, "{", line}; }
    if (c == '}') { ++pos; return {TokenType::RBRACE, "}", line}; }
    if (c == ';') { ++pos; return {TokenType::SEMICOLON, ";", line}; }
    if (c == ',') { ++pos; return {TokenType::COMMA, ",", line}; }
    if (c == ':') { ++pos; return {TokenType::COLON, ":", line}; }
    if (c == '&') { ++pos; return {TokenType::OP_AND, "&", line}; }
    if (c == '|') { ++pos; return {TokenType::OP_OR, "|", line}; }
    if (c == '^') { ++pos; return {TokenType::OP_XOR, "^", line}; }
    if (c == '+') { ++pos; return {TokenType::OP_PLUS, "+", line}; }
    if (c == '/') { ++pos; return {TokenType::OP_DIV, "/", line}; }

    if (c == '!' && pos + 1 < source.size() && source[pos + 1] == '=') {
        pos += 2; return {TokenType::OP_NEQ, "!=", line};
    }
    if (c == '!') { ++pos; return {TokenType::OP_NOT, "!", line}; }

    if (c == '<' && pos + 1 < source.size() && source[pos + 1] == '=') {
        pos += 2; return {TokenType::ASSIGN_LE, "<=", line};
    }
    if (c == '=' && pos + 1 < source.size() && source[pos + 1] == '=') {
        pos += 2; return {TokenType::OP_EQ, "==", line};
    }
    if (c == '=') { ++pos; return {TokenType::EQUALS, "=", line}; }

    if (isdigit(c)) {
        std::string num;
        while (pos < source.size() && isdigit(source[pos])) num += source[pos++];
        return {TokenType::NUMBER, num, line};
    }

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
        if (id == "always") return {TokenType::KW_ALWAYS, id, line};
        if (id == "posedge") return {TokenType::KW_POSEDGE, id, line};
        if (id == "begin") return {TokenType::KW_BEGIN, id, line};
        if (id == "end") return {TokenType::KW_END, id, line};
        if (id == "endmodule") return {TokenType::KW_ENDMODULE, id, line};
        if (id == "else") return {TokenType::KW_ELSE, id, line};
        if (id == "if") return {TokenType::KW_IF, id, line};

        if (id.size() > 2 && id[0] == '8' && id[1] == '\'') {
            char fmt = id[2];
            std::string val = id.substr(3);
            if (fmt == 'h' || fmt == 'H') {
                return {TokenType::HEX_NUMBER, val, line};
            } else if (fmt == 'b' || fmt == 'B') {
                uint32_t n = 0;
                for (char bit : val) n = (n << 1) | (bit == '1' ? 1 : 0);
                return {TokenType::NUMBER, std::to_string(n), line};
            } else if (fmt == 'd' || fmt == 'D') {
                return {TokenType::NUMBER, val, line};
            }
        }

        return {TokenType::IDENT, id, line};
    }

    fprintf(stderr, "[Lexer] Line %zu: unrecognized character '%c'\n", line, c);
    ++pos;
    return next_token(source, pos, line);
}

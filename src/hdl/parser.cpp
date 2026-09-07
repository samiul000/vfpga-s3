#include "parser.h"
#include <cstdio>
#include <cstdlib>

const Token &Parser::peek() {
    static Token eof{TokenType::END_OF_FILE, "", 0};
    return pos_ < tokens_->size() ? (*tokens_)[pos_] : eof;
}

const Token &Parser::advance() {
    return (*tokens_)[pos_++];
}

bool Parser::check(TokenType type) {
    return pos_ < tokens_->size() && (*tokens_)[pos_].type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { ++pos_; return true; }
    return false;
}

void Parser::expect(TokenType type, const char *msg) {
    if (!match(type)) {
        fprintf(stderr, "[Parser] Line %zu: expected %s, got '%s'\n",
                peek().line, msg, peek().text.c_str());
    }
}

int32_t Parser::parse_number() {
    const Token &t = advance();
    if (t.type == TokenType::HEX_NUMBER) return (int32_t)strtol(t.text.c_str(), nullptr, 16);
    return (int32_t)strtol(t.text.c_str(), nullptr, 10);
}

AstNode Parser::parse_module() {
    AstNode node;
    node.type = AstNode::Type::MODULE;
    expect(TokenType::KW_MODULE, "module");
    node.name = advance().text;
    match(TokenType::SEMICOLON);
    return node;
}

AstNode Parser::parse_port_decl() {
    AstNode node;
    bool is_output = check(TokenType::KW_OUTPUT);
    if (is_output) { advance(); node.type = AstNode::Type::OUTPUT; }
    else { advance(); node.type = AstNode::Type::INPUT; }

    if (match(TokenType::LBRACKET)) {
        node.msb = parse_number();
        expect(TokenType::COLON, ":");
        node.lsb = parse_number();
        expect(TokenType::RBRACKET, "]");
    }

    node.name = advance().text;
    expect(TokenType::SEMICOLON, ";");
    return node;
}

AstNode Parser::parse_wire_decl() {
    AstNode node;
    node.type = AstNode::Type::WIRE;
    advance(); // wire

    if (match(TokenType::LBRACKET)) {
        node.msb = parse_number();
        expect(TokenType::COLON, ":");
        node.lsb = parse_number();
        expect(TokenType::RBRACKET, "]");
    }

    node.name = advance().text;
    expect(TokenType::SEMICOLON, ";");
    return node;
}

AstNode Parser::parse_register_decl() {
    AstNode node;
    node.type = AstNode::Type::REGISTER;
    advance(); // register

    if (match(TokenType::LBRACKET)) {
        node.msb = parse_number();
        expect(TokenType::COLON, ":");
        node.lsb = parse_number();
        expect(TokenType::RBRACKET, "]");
    }

    node.name = advance().text;
    expect(TokenType::SEMICOLON, ";");
    return node;
}

AstNode Parser::parse_primary() {
    if (check(TokenType::NUMBER) || check(TokenType::HEX_NUMBER)) {
        AstNode node;
        node.type = check(TokenType::HEX_NUMBER) ? AstNode::Type::EXPR_HEX : AstNode::Type::EXPR_NUMBER;
        node.name = advance().text;
        return node;
    }

    if (check(TokenType::IDENT)) {
        AstNode node;
        node.type = AstNode::Type::EXPR_IDENT;
        node.name = advance().text;

        if (match(TokenType::LBRACKET)) {
            if (check(TokenType::NUMBER) || check(TokenType::HEX_NUMBER)) {
                int32_t idx = parse_number();
                if (match(TokenType::COLON)) {
                    int32_t lsb = parse_number();
                    node.type = AstNode::Type::RANGE_SELECT;
                    node.msb = idx;
                    node.lsb = lsb;
                } else {
                    node.type = AstNode::Type::BIT_SELECT;
                    node.bit_index = idx;
                }
            }
            expect(TokenType::RBRACKET, "]");
        }
        return node;
    }

    if (match(TokenType::LBRACE)) {
        AstNode node;
        node.type = AstNode::Type::CONCAT;
        while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
            node.children.push_back(advance().text);
            if (check(TokenType::COMMA)) advance();
        }
        expect(TokenType::RBRACE, "}");
        return node;
    }

    if (match(TokenType::LPAREN)) {
        AstNode node = parse_expr();
        expect(TokenType::RPAREN, ")");
        return node;
    }

    AstNode node;
    node.type = AstNode::Type::EXPR_IDENT;
    node.name = advance().text;
    return node;
}

AstNode Parser::parse_unary_expr() {
    if (match(TokenType::OP_NOT)) {
        AstNode node;
        node.type = AstNode::Type::EXPR_NOT;
        node.op = "!";
        node.child_idx.push_back(-1);
        AstNode child = parse_primary();
        node.children.push_back(child.name);
        return node;
    }
    return parse_primary();
}

AstNode Parser::parse_expr() {
    AstNode left = parse_unary_expr();

    while (check(TokenType::OP_AND) || check(TokenType::OP_OR) ||
           check(TokenType::OP_XOR) || check(TokenType::OP_PLUS) ||
           check(TokenType::OP_EQ) || check(TokenType::OP_NEQ)) {
        AstNode node;
        node.type = AstNode::Type::EXPR_BINARY;
        node.op = advance().text;
        AstNode right = parse_unary_expr();
        node.children.push_back(left.name);
        node.children.push_back(right.name);
        node.msb = left.bit_index;
        left = node;
    }
    return left;
}

AstNode Parser::parse_assign_le() {
    AstNode node;
    node.type = AstNode::Type::ASSIGN_LE;
    advance(); // <=

    AstNode expr = parse_expr();
    node.op = expr.op;
    // Copy all children from expression (handles binary ops like count + 1)
    if (!expr.children.empty()) {
        node.children = expr.children;
    } else if (!expr.name.empty()) {
        node.children.push_back(expr.name);
    }

    if (check(TokenType::SEMICOLON)) advance();
    return node;
}

AstNode Parser::parse_if() {
    AstNode node;
    node.type = AstNode::Type::IF;
    advance(); // if

    expect(TokenType::LPAREN, "(");
    AstNode cond = parse_expr();
    if (cond.type == AstNode::Type::EXPR_NOT && !cond.children.empty()) {
        node.children.push_back(cond.children[0]);
        node.op = "!";
    } else {
        node.children.push_back(cond.name);
        node.op = cond.op;
    }
    expect(TokenType::RPAREN, ")");

    if (check(TokenType::KW_BEGIN)) {
        AstNode then_block = parse_begin_block();
        node.sub_nodes.push_back(then_block);
        node.child_idx.push_back(-1);
    } else {
        AstNode target = parse_primary();
        AstNode then_stmt = parse_assign_le();
        then_stmt.name = target.name;
        if (!target.children.empty()) then_stmt.children = target.children;
        node.sub_nodes.push_back(then_stmt);
    }

    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_BEGIN)) {
            AstNode else_block = parse_begin_block();
            node.sub_nodes.push_back(else_block);
        } else {
            AstNode target2 = parse_primary();
            AstNode else_stmt = parse_assign_le();
            else_stmt.name = target2.name;
            if (!target2.children.empty()) else_stmt.children = target2.children;
            node.sub_nodes.push_back(else_stmt);
        }
    }
    return node;
}

AstNode Parser::parse_begin_block() {
    AstNode node;
    node.type = AstNode::Type::BEGIN_END;
    advance(); // begin

    while (!check(TokenType::KW_END) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::KW_IF)) {
            AstNode child = parse_if();
            node.sub_nodes.push_back(child);
        } else if (check(TokenType::IDENT) || check(TokenType::LBRACKET)) {
            AstNode target = parse_primary();
            AstNode stmt = parse_assign_le();
            stmt.name = target.name;
            if (!target.children.empty()) stmt.children = target.children;
            node.sub_nodes.push_back(stmt);
        } else {
            advance();
        }
    }
    expect(TokenType::KW_END, "end");
    return node;
}

AstNode Parser::parse_always() {
    AstNode node;
    node.type = AstNode::Type::ALWAYS_POSEDGE;
    advance(); // always

    match(TokenType::AT); // skip @
    expect(TokenType::LPAREN, "(");
    match(TokenType::KW_POSEDGE);
    node.name = advance().text;
    expect(TokenType::RPAREN, ")");
    expect(TokenType::KW_BEGIN, "begin");

    while (!check(TokenType::KW_END) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::KW_IF)) {
            AstNode if_node = parse_if();
            node.sub_nodes.push_back(if_node);
        } else if (check(TokenType::IDENT) || check(TokenType::LBRACKET)) {
            AstNode target = parse_primary();
            AstNode stmt = parse_assign_le();
            stmt.name = target.name;
            if (!target.children.empty()) stmt.children = target.children;
            node.sub_nodes.push_back(stmt);
        } else {
            advance();
        }
    }
    expect(TokenType::KW_END, "end");
    return node;
}

std::vector<AstNode> Parser::parse(const std::vector<Token> &tokens) {
    std::vector<AstNode> ast;
    tokens_ = &tokens;
    pos_ = 0;

    while (pos_ < tokens.size()) {
        if (check(TokenType::KW_MODULE)) { ast.push_back(parse_module()); }
        else if (check(TokenType::KW_INPUT)) { ast.push_back(parse_port_decl()); }
        else if (check(TokenType::KW_OUTPUT)) { ast.push_back(parse_port_decl()); }
        else if (check(TokenType::KW_WIRE)) { ast.push_back(parse_wire_decl()); }
        else if (check(TokenType::KW_REGISTER)) { ast.push_back(parse_register_decl()); }
        else if (check(TokenType::KW_ASSIGN)) {
            advance(); // assign
            AstNode target = parse_primary();
            match(TokenType::EQUALS);
            AstNode expr = parse_expr();
            AstNode node;
            node.type = AstNode::Type::ASSIGN;
            node.name = target.name;
            node.children = expr.children;
            if (node.children.empty() && !expr.name.empty()) {
                node.children.push_back(expr.name);
            }
            node.op = expr.op;
            if (check(TokenType::SEMICOLON)) advance();
            ast.push_back(node);
        }
        else if (check(TokenType::KW_ALWAYS)) { ast.push_back(parse_always()); }
        else if (check(TokenType::KW_ENDMODULE)) { advance(); }
        else { advance(); }
    }
    return ast;
}

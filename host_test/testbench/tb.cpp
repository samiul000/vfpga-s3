// Testbench DSL lexer/parser. See tb.h.
#include "tb.h"

#include <cctype>
#include <cstdio>

namespace {

struct Tok {
    enum T { IDENT, NUMBER, EQ, EQEQ, NEQ, LT, GT, LEQ, GEQ,
             LBRACE, RBRACE, LPAREN, RPAREN, SEMI, COMMA, END } t = END;
    std::string text;
    size_t line = 0;
};

struct Lexer {
    const std::string &s;
    size_t pos = 0, line = 1;
    explicit Lexer(const std::string &src) : s(src) {}

    std::vector<Tok> run() {
        std::vector<Tok> out;
        while (pos < s.size()) {
            char c = s[pos];
            if (c == '\n') { ++line; ++pos; continue; }
            if (isspace((unsigned char)c)) { ++pos; continue; }
            if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '/') {
                while (pos < s.size() && s[pos] != '\n') ++pos;
                continue;
            }
            Tok tk;
            tk.line = line;
            if (isalpha((unsigned char)c) || c == '_') {
                while (pos < s.size() && (isalnum((unsigned char)s[pos]) || s[pos] == '_'))
                    tk.text += s[pos++];
                // duration suffix: 20ns
                if ((s[pos] == 'n' || s[pos] == 'N') && pos + 1 < s.size() &&
                    (s[pos + 1] == 's' || s[pos + 1] == 'S') && !tk.text.empty() &&
                    isdigit((unsigned char)tk.text[0])) {
                    // number+unit glued: split in parser via text check
                    tk.text += s[pos++]; tk.text += s[pos++];
                }
                tk.t = Tok::IDENT;
                out.push_back(tk);
                continue;
            }
            if (isdigit((unsigned char)c)) {
                while (pos < s.size() && isdigit((unsigned char)s[pos]))
                    tk.text += s[pos++];
                if (pos + 1 < s.size() && (s[pos] == 'n' || s[pos] == 'N') &&
                    (s[pos + 1] == 's' || s[pos + 1] == 'S')) {
                    tk.text += s[pos++]; tk.text += s[pos++];
                }
                tk.t = Tok::NUMBER;
                out.push_back(tk);
                continue;
            }
            switch (c) {
                case '{': tk.t = Tok::LBRACE; ++pos; break;
                case '}': tk.t = Tok::RBRACE; ++pos; break;
                case '(': tk.t = Tok::LPAREN; ++pos; break;
                case ')': tk.t = Tok::RPAREN; ++pos; break;
                case ';': tk.t = Tok::SEMI; ++pos; break;
                case ',': tk.t = Tok::COMMA; ++pos; break;
                case '=':
                    if (pos + 1 < s.size() && s[pos + 1] == '=') {
                        tk.t = Tok::EQEQ; tk.text = "=="; pos += 2;
                    } else { tk.t = Tok::EQ; tk.text = "="; ++pos; }
                    break;
                case '!':
                    if (pos + 1 < s.size() && s[pos + 1] == '=') {
                        tk.t = Tok::NEQ; tk.text = "!="; pos += 2;
                    } else { tk.t = Tok::END; tk.text = "!"; ++pos; }
                    break;
                case '<':
                    if (pos + 1 < s.size() && s[pos + 1] == '=') {
                        tk.t = Tok::LEQ; tk.text = "<="; pos += 2;
                    } else { tk.t = Tok::LT; tk.text = "<"; ++pos; }
                    break;
                case '>':
                    if (pos + 1 < s.size() && s[pos + 1] == '=') {
                        tk.t = Tok::GEQ; tk.text = ">="; pos += 2;
                    } else { tk.t = Tok::GT; tk.text = ">"; ++pos; }
                    break;
                default: ++pos; continue;  // skip unknown chars (parser errors later)
            }
            out.push_back(tk);
        }
        Tok e; e.t = Tok::END; e.line = line;
        out.push_back(e);
        return out;
    }
};

struct Parser {
    std::vector<Tok> toks;
    size_t pos = 0;
    const char *path;
    std::vector<std::string> errors;

    explicit Parser(const std::vector<Tok> &t, const char *p) : toks(t), path(p) {}

    const Tok &peek() { return toks[pos]; }
    const Tok &next() { return toks[pos++]; }

    void fail(size_t ln, const std::string &msg) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s:%lu: error: %s", path,
                 (unsigned long)ln, msg.c_str());
        errors.push_back(buf);
    }

    bool at_ident(const char *kw) {
        return peek().t == Tok::IDENT && peek().text == kw;
    }
    bool consume_ident(const char *kw, size_t ln) {
        if (at_ident(kw)) { next(); return true; }
        fail(ln, std::string("expected '") + kw + "'");
        return false;
    }
    bool expect(Tok::T t, const char *what, size_t ln) {
        if (peek().t == t) { next(); return true; }
        fail(ln, std::string("expected ") + what);
        return false;
    }

    // Parse "period=10ns" style attr; returns value in ns for durations.
    bool duration_ns(uint64_t &ns) {
        if (peek().t != Tok::NUMBER) return false;
        std::string t = next().text;
        if (t.size() < 3 || t.substr(t.size() - 2) != "ns") return false;
        ns = (uint64_t)strtoull(t.substr(0, t.size() - 2).c_str(), nullptr, 10);
        return true;
    }

    bool stmt(std::vector<TbCmd> &out) {
        if (peek().t != Tok::IDENT) {
            fail(peek().line, "expected command");
            return false;
        }
        std::string kw = next().text;
        size_t ln = toks[pos - 1].line;
        TbCmd c;
        c.line = ln;
        if (kw == "timescale") {
            uint64_t ns = 0;
            if (!duration_ns(ns)) { fail(ln, "expected duration after 'timescale'"); return false; }
            c.type = TbCmd::Type::TIMESCALE;
            c.value = ns;
        } else if (kw == "clock") {
            if (peek().t != Tok::IDENT) { fail(ln, "expected signal after 'clock'"); return false; }
            c.name = next().text;
            c.type = TbCmd::Type::CLOCK;
            // attrs: period=10ns duty=50 init=0 (any order, all optional)
            while (peek().t == Tok::IDENT) {
                std::string a = next().text;
                if (!expect(Tok::EQ, "'='", ln)) return false;
                if (a == "period") {
                    if (!duration_ns(c.period_ns)) { fail(ln, "expected duration after 'period='"); return false; }
                } else if (a == "duty") {
                    if (peek().t != Tok::NUMBER) { fail(ln, "expected number after 'duty='"); return false; }
                    c.duty_pct = atoi(next().text.c_str());
                } else if (a == "init") {
                    if (peek().t != Tok::NUMBER) { fail(ln, "expected 0/1 after 'init='"); return false; }
                    c.init = atoi(next().text.c_str()) ? 1 : 0;
                } else { fail(ln, "unknown clock attribute '" + a + "'"); return false; }
            }
        } else if (kw == "reset") {
            if (peek().t != Tok::IDENT) { fail(ln, "expected signal after 'reset'"); return false; }
            c.name = next().text;
            if (peek().t != Tok::IDENT) { fail(ln, "expected active_high/active_low"); return false; }
            std::string lvl = next().text;
            if (lvl != "active_high" && lvl != "active_low") {
                fail(ln, "expected active_high/active_low"); return false;
            }
            c.type = TbCmd::Type::RESET;
            c.active_high = (lvl == "active_high");
        } else if (kw == "drive") {
            if (peek().t != Tok::IDENT) { fail(ln, "expected signal after 'drive'"); return false; }
            c.name = next().text;
            if (!expect(Tok::EQ, "'='", ln)) return false;
            if (peek().t != Tok::NUMBER) { fail(ln, "expected value after '='"); return false; }
            c.value = (uint64_t)strtoull(next().text.c_str(), nullptr, 10);
            c.type = TbCmd::Type::DRIVE;
        } else if (kw == "wait") {
            if (peek().t == Tok::NUMBER || peek().t == Tok::IDENT) {
                // rising_edge(clk) / falling_edge(clk) or duration
                if (peek().t == Tok::IDENT &&
                    (peek().text == "rising_edge" || peek().text == "falling_edge")) {
                    c.rising = (next().text == "rising_edge");
                    if (!expect(Tok::LPAREN, "'('", ln)) return false;
                    if (peek().t != Tok::IDENT) { fail(ln, "expected signal in edge wait"); return false; }
                    c.name = next().text;
                    if (!expect(Tok::RPAREN, "')'", ln)) return false;
                    c.type = TbCmd::Type::WAIT_EDGE;
                } else if (!duration_ns(c.value)) {
                    fail(ln, "expected duration after 'wait'"); return false;
                } else {
                    c.type = TbCmd::Type::WAIT_NS;
                }
            } else { fail(ln, "expected duration after 'wait'"); return false; }
        } else if (kw == "repeat") {
            if (peek().t != Tok::NUMBER) { fail(ln, "expected count after 'repeat'"); return false; }
            c.value = (uint64_t)strtoull(next().text.c_str(), nullptr, 10);
            if (!expect(Tok::LBRACE, "'{'", ln)) return false;
            c.type = TbCmd::Type::REPEAT;
            while (peek().t != Tok::RBRACE && peek().t != Tok::END) {
                if (!stmt(c.block)) return false;
            }
            if (!expect(Tok::RBRACE, "'}'", ln)) return false;
            out.push_back(c);
            return true;  // repeat body has no ';'
        } else if (kw == "assert") {
            if (peek().t != Tok::IDENT) { fail(ln, "expected signal after 'assert'"); return false; }
            c.name = next().text;
            Tok::T ot = peek().t;
            if (ot != Tok::EQEQ && ot != Tok::NEQ && ot != Tok::LT &&
                ot != Tok::GT && ot != Tok::LEQ && ot != Tok::GEQ) {
                fail(ln, "expected comparison operator"); return false;
            }
            c.op = next().text;
            if (peek().t != Tok::NUMBER) { fail(ln, "expected value in assert"); return false; }
            c.value = (uint64_t)strtoull(next().text.c_str(), nullptr, 10);
            c.type = TbCmd::Type::ASSERT;
        } else if (kw == "trace") {
            if (peek().t != Tok::IDENT) { fail(ln, "expected signal after 'trace'"); return false; }
            c.name = next().text;
            c.type = TbCmd::Type::TRACE;
        } else if (kw == "stop") {
            c.type = TbCmd::Type::STOP;
        } else {
            fail(ln, "unknown command '" + kw + "'");
            return false;
        }
        if (!expect(Tok::SEMI, "';'", ln)) return false;
        out.push_back(c);
        return true;
    }
};

}  // namespace

bool parse_testbench(const std::string &src, const char *path,
                     Testbench &tb, std::vector<std::string> &errors) {
    Lexer lex(src);
    std::vector<Tok> toks = lex.run();
    Parser p(toks, path);
    if (!p.at_ident("testbench")) {
        p.fail(p.peek().line, "expected 'testbench'");
        errors = p.errors;
        return false;
    }
    p.next();
    if (p.peek().t != Tok::IDENT) {
        p.fail(p.peek().line, "expected testbench name");
        errors = p.errors;
        return false;
    }
    tb.name = p.next().text;
    if (!p.expect(Tok::LBRACE, "'{'", p.peek().line)) {
        errors = p.errors;
        return false;
    }
    while (p.peek().t != Tok::RBRACE && p.peek().t != Tok::END) {
        if (!p.stmt(tb.cmds)) {
            errors = p.errors;
            return false;
        }
    }
    if (!p.expect(Tok::RBRACE, "'}'", p.peek().line)) {
        errors = p.errors;
        return false;
    }
    return true;
}

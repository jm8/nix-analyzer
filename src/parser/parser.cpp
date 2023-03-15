#include "parser.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/parser-tab.hh>
#include <nix/symbol-table.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include "common/analysis.h"
#include "common/position.h"
#include "parser/tokenizer.h"
#include "schema/schema.h"

struct Parser {
    nix::EvalState& state;
    Analysis& analysis;
    std::string source;
    Position targetPos;
    bool justReportedError = false;

    Tokenizer tokenizer = Tokenizer{state, analysis.path, source};
    // tokens[0] == previous()
    // tokens[1] == current() == lookahead(0)
    // tokens[2...] == lookahead(1), lookahead(2), ...
    // tokens.size() >= 4 but may be more
    std::deque<Token> tokens;

    Parser(
        nix::EvalState& state,
        Analysis& analysis,
        std::string source,
        Position targetPos
    )
        : state(state),
          analysis(analysis),
          source(source),
          targetPos(targetPos),
          tokenizer(state, analysis.path, source),
          tokens(4) {
        tokens[0].type = YYEOF;
        tokens[1] = tokenizer.advance();
        tokens[2] = tokenizer.advance();
        tokens[3] = tokenizer.advance();
    }

    Token previous() { return tokens[0]; }
    Token current() { return tokens[1]; }
    Token lookahead(size_t i) {
        auto array_index = i + 1;
        while (tokens.size() <= array_index) {
            tokens.push_back(tokenizer.advance());
        }
        return tokens[array_index];
    }

    bool lookaheadMatches(std::vector<TokenType> types) {
        for (size_t i = 0; i < types.size(); i++) {
            if (lookahead(i).type != types[i])
                return false;
        }
        return true;
    }

    Token consume() {
        auto result = current();
        tokens.pop_front();
        if (tokens.size() < 4) {
            tokens.push_back(tokenizer.advance());
        }
        return result;
    }

    std::optional<Token> allow(TokenType type) {
        if (current().type == type) {
            return current();
        }
        return {};
    }

    std::optional<Token> allow(const std::vector<TokenType>& types) {
        for (auto type : types) {
            if (auto result = allow(type))
                return result;
        }
        return {};
    }

    std::optional<Token> accept(TokenType type) {
        if (allow(type)) {
            justReportedError = false;
            return consume();
        }
        return {};
    }

    std::optional<Token> expect(TokenType type) {
        if (allow(type)) {
            justReportedError = false;
            return consume();
        }
        error(
            "expected '" + tokenName(type) + "', got '" +
                tokenName(current().type) + "'",
            current().range
        );
        return {};
    }

    nix::PosIdx posIdx(Position position) {
        return state.positions.add(
            {analysis.path, nix::foFile}, position.line + 1, position.col + 1
        );
    }

    nix::Expr* missing() {
        auto e = new nix::ExprVar(state.symbols.create("null"));
        return e;
    }

    void error(std::string msg, Range range) {
        if (justReportedError)
            return;
        analysis.parseErrors.push_back({msg, range});
        justReportedError = true;
    }

    void visit(nix::Expr* e, Range range) {
        if (range.contains(targetPos)) {
            analysis.exprPath.push_back({e});
        }
    }

    // GRAMMAR

    nix::Expr* expr() {
        // ID ':' expr_function
        if (lookaheadMatches({ID, ':'})) {
            auto start = current().range.start;
            auto arg = accept(ID);
            auto argSym = state.symbols.create(get<std::string>(arg->val));
            accept(':');
            auto body = expr();
            auto end = previous().range.end;
            auto result =
                new nix::ExprLambda(posIdx(start), argSym, nullptr, body);
            visit(result, {start, end});
            return result;
        }
        // '{' formals '}' ':' expr_function
        // '{' formals '}' '@' ID ':' expr_function
        if (lookaheadMatches({'{', ID, ','}) ||
            lookaheadMatches({'{', ID, '?'}) ||
            lookaheadMatches({'{', '}', ':'}) ||
            lookaheadMatches({'{', '}', '@'})) {
            auto start = current().range.start;
            accept('{');
            auto fs = formals();
            expect('}');
            nix::Symbol arg;
            if (accept('@')) {
                arg = state.symbols.create(get<std::string>(expect(ID)->val));
                if (previous().range.contains(targetPos)) {
                    analysis.arg = true;
                }
            }
            expect(':');
            auto body = expr();
            auto end = previous().range.end;
            auto e = new nix::ExprLambda(
                posIdx(start), arg, to_formals(fs, arg), body
            );
            visit(e, {start, end});
            return e;
        }
        // ID '@' '{' formals '}' ':' expr_function
        if (lookaheadMatches({ID, '@'})) {
            auto start = current().range.start;
            nix::Symbol arg =
                state.symbols.create(get<std::string>(expect(ID)->val));
            if (previous().range.contains(targetPos)) {
                analysis.arg = true;
            }
            expect('@');
            expect('{');
            auto fs = formals();
            expect('}');
            expect(':');
            auto body = expr();
            auto end = previous().range.end;
            auto e = new nix::ExprLambda(
                posIdx(start), arg, to_formals(fs, arg), body
            );
            visit(e, {start, end});
            return e;
        }
        if (allow(allowedKeywordExprStarts)) {
            return keyword_expression(true);
        }
        return expr_app();
    }

    // these are assert, with, let, if.
    nix::Expr* keyword_expression(bool allowed) {
        if (!allowed) {
            error(
                tokenName(current().type) + " not allowed here", current().range
            );
        }
        nix::Expr* result;
        auto start = current().range.start;
        if (accept(ASSERT)) {
            auto cond = expr();
            expect(';');
            auto body = expr();
            result = new nix::ExprAssert(posIdx(start), cond, body);
        }
        auto end = previous().range.end;
        visit(result, {start, end});
        return result;
    }

    nix::Expr* expr_app() {
        auto start = current().range.start;
        auto f = expr_select();

        if (allow(allowedExprStarts)) {
            auto call = new nix::ExprCall(posIdx(start), f, {});
            while (allow(allowedExprStarts)) {
                // we want
                // { a = a b c d.e.f = 2; }
                // to be parsed as
                // { a = a b c; d.e.f = 2; }
                size_t i = 0;
                // check if looking at potential attrpath
                while (lookahead(i).type == ID) {
                    if (lookahead(i + 1).type == '.') {
                        i += 2;
                    } else {
                        i += 1;
                        break;
                    }
                }
                if (lookahead(i).type == '=') {
                    break;
                }
                call->args.push_back(expr_select());
            }
            auto end = previous().range.end;
            visit(call, {start, end});
            return call;
        } else {
            return f;
        }
    }

    nix::Expr* expr_select() {
        auto start = current().range.start;
        auto e = expr_simple();
        if (accept('.')) {
            auto path = attrPath();
            auto select = new nix::ExprSelect(posIdx(start), e, *path, nullptr);
            auto end = previous().range.end;
            visit(select, {start, end});
            return select;
        } else {
            return e;
        }
    }

    nix::Expr* expr_simple() {
        auto start = current().range.start;
        auto e = expr_simple_();
        auto end = previous().range.end;
        visit(e, {start, end});
        return e;
    }

    const std::vector<TokenType> allowedKeywordExprStarts{
        ASSERT,
        WITH,
        LET,
        IF};
    const std::vector<TokenType>
        allowedExprStarts{ASSERT, WITH, LET, IF, ID, INT, '{'};

    nix::Expr* expr_simple_() {
        // ID
        if (!allow(allowedExprStarts)) {
            error("expected expression", current().range);
            while (!allow({';', '}'})) {
                consume();
            }
            return missing();
        }
        if (auto id = accept(ID)) {
            std::string_view name = get<std::string>(id->val);
            if (name == "__curPos") {
                return new nix::ExprPos(posIdx(id->range.start));
            } else {
                return new nix::ExprVar(
                    posIdx(id->range.start), state.symbols.create(name)
                );
            }
        }
        // INT
        if (auto token = accept(INT)) {
            return new nix::ExprInt(get<nix::NixInt>(token->val));
        }
        // '{' binds '}'
        if (accept('{')) {
            auto e = binds();
            expect('}');
            return e;
        }
        assert(false);
    }

    // copy+paste from parser.y
    void addAttr(
        nix::ExprAttrs* attrs,
        nix::AttrPath& attrPath,
        nix::Expr* e,
        Range range
    ) {
        const nix::PosIdx pos = posIdx(range.start);
        nix::AttrPath::iterator i;
        // All attrpaths have at least one attr
        assert(!attrPath.empty());
        // Checking attrPath validity.
        // ===========================
        for (i = attrPath.begin(); i + 1 < attrPath.end(); i++) {
            if (i->symbol) {
                auto j = attrs->attrs.find(i->symbol);
                if (j != attrs->attrs.end()) {
                    if (!j->second.inherited) {
                        auto attrs2 =
                            dynamic_cast<nix::ExprAttrs*>(j->second.e);
                        if (!attrs2) {
                            error("duplicate attr", range);
                            // dupAttr(state, attrPath, start, end,
                            // j->second.pos);
                            return;
                        }
                        attrs = attrs2;
                    } else {
                        error("duplicate attr", range);
                        // dupAttr(state, attrPath, start, end,
                        // j->second.pos);
                        return;
                    }
                } else {
                    auto nested = new nix::ExprAttrs;
                    attrs->attrs[i->symbol] =
                        nix::ExprAttrs::AttrDef(nested, pos);
                    attrs = nested;
                }
            } else {
                auto nested = new nix::ExprAttrs;
                attrs->dynamicAttrs.push_back(
                    nix::ExprAttrs::DynamicAttrDef(i->expr, nested, pos)
                );
                attrs = nested;
            }
        }
        // Expr insertion.
        // ==========================
        if (i->symbol) {
            auto j = attrs->attrs.find(i->symbol);
            if (j != attrs->attrs.end()) {
                // This attr path is already defined. However, if both
                // e and the expr pointed by the attr path are two attribute
                // sets, we want to merge them. Otherwise, throw an error.
                auto ae = dynamic_cast<nix::ExprAttrs*>(e);
                auto jAttrs = dynamic_cast<nix::ExprAttrs*>(j->second.e);
                if (jAttrs && ae) {
                    for (auto& ad : ae->attrs) {
                        auto j2 = jAttrs->attrs.find(ad.first);
                        if (j2 !=
                            jAttrs->attrs.end()) {  // Attr already defined
                                                    // in iAttrs, error.
                            error("duplicate attr", range);
                            // dupAttr(state, ad.first, start, end,
                            // ad.second.pos);
                            return;
                        }
                        jAttrs->attrs.emplace(ad.first, ad.second);
                    }
                } else {
                    error("duplicate attr", range);
                    // dupAttr(state, attrPath, start, end, j->second.pos);
                    return;
                }
            } else {
                // This attr path is not defined. Let's create it.
                attrs->attrs.emplace(
                    i->symbol, nix::ExprAttrs::AttrDef(e, pos)
                );
                e->setName(i->symbol);
            }
        } else {
            attrs->dynamicAttrs.push_back(
                nix::ExprAttrs::DynamicAttrDef(i->expr, e, pos)
            );
        }
    }

    nix::Expr* binds() {
        auto attrs = new nix::ExprAttrs;
        while (allow(ID) || allow(OR_KW)) {
            auto start = current().range.start;
            auto path = attrPath();
            if (!expect('=')) {
                continue;
            }
            auto e = expr();
            auto end = previous().range.end;
            addAttr(attrs, *path, e, {start, end});
            if (!expect(';')) {
                continue;
            }
        }
        return attrs;
    }

    nix::AttrPath* attrPath() {
        auto path = new nix::AttrPath;

        while (true) {
            auto start = current().range.start;
            if (auto id = accept(ID)) {
                auto name = get<std::string>(id->val);
                path->push_back(state.symbols.create(name));
            } else if (accept(OR_KW)) {
                path->push_back(state.symbols.create("or"));
            } else {
                error("expected ID", current().range);
                auto dotPosition = previous().range.start;
                auto nextTokenPosition = current().range.start;
                path->push_back(state.symbols.create(""));
                if (Range{dotPosition, nextTokenPosition}.contains(targetPos)) {
                    analysis.attr = {path->size() - 1, path};
                }
                break;
            }
            auto end = previous().range.end;
            if (Range{start, end}.contains(targetPos)) {
                analysis.attr = {path->size() - 1, path};
            }
            if (!accept('.')) {
                break;
            }
        }

        return path;
    }

    struct ParserFormal {
        nix::Formal formal;
        Range range;

        bool operator<(ParserFormal other) {
            return formal.name < other.formal.name ||
                   range.start < other.range.start;
        }
    };

    struct ParserFormals {
        std::vector<ParserFormal> formals;
        bool ellipsis = false;
    };

    ParserFormals* formals() {
        auto result = new ParserFormals;
        while (allow({ELLIPSIS, ID})) {
            if (accept(ELLIPSIS)) {
                result->ellipsis = true;
            } else {
                auto id = accept(ID);

                auto name = state.symbols.create(get<std::string>(id->val));
                nix::Formal formal{posIdx(id->range.start), name, nullptr};
                if (accept('?')) {
                    formal.def = expr();
                }
                if (id->range.contains(targetPos)) {
                    analysis.formal = formal;
                }
                result->formals.push_back({formal, id->range});
            }
            if (!allow('}')) {
                expect(',');
            }
        }
        return result;
    }

    nix::Formals* to_formals(ParserFormals* formals, nix::Symbol arg) {
        std::sort(formals->formals.begin(), formals->formals.end());

        nix::Formals result;
        result.ellipsis = formals->ellipsis;

        if (formals->formals.empty()) {
            return new nix::Formals(std::move(result));
        }

        for (auto [formal, range] : formals->formals) {
            // nix-analyzer: report duplicate formals error without throwing
            // exception
            if (!result.formals.empty() &&
                (formal.name == result.formals.back().name || formal.name == arg
                )) {
                error("duplicate formal function argument", range);
            } else {
                result.formals.push_back(formal);
            }
        }

        delete formals;
        return new nix::Formals(std::move(result));
    }
};

Analysis parse(
    nix::EvalState& state,
    std::string source,
    nix::Path path,
    nix::Path basePath,
    Position targetPos
) {
    Analysis analysis;

    Parser parser{state, analysis, source, targetPos};
    auto e = parser.expr();
    if (analysis.exprPath.empty()) {
        analysis.exprPath.push_back(e);
    }

    return analysis;
}

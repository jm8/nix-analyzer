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
#include <iostream>
#include <optional>
#include <string_view>
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
    std::array<Token, 4> tokens;

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
          tokenizer(state, analysis.path, source) {
        for (int i = 0; i < 3; i++)
            consume();
    }

    Token previous() { return tokens[0]; }
    Token current() { return tokens[1]; }
    Token lookahead() { return tokens[2]; }
    Token lookahead2() { return tokens[3]; }

    Token consume() {
        auto result = current();
        tokens[0] = tokens[1];
        tokens[1] = tokens[2];
        tokens[2] = tokens[3];
        tokens[3] = tokenizer.advance();
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
        auto e = expr_app();

        return e;
    }

    nix::Expr* expr_app() {
        auto start = current().range.start;
        auto f = expr_select();

        if (allow(allowedExprStarts)) {
            auto call = new nix::ExprCall(posIdx(start), f, {});
            while (allow(allowedExprStarts)) {
                call->args.push_back(expr());
            }
            auto end = current().range.end;
            visit(call, {start, end});
            return call;
        } else {
            return f;
        }
    }

    nix::Expr* expr_select() { return expr_simple(); }

    nix::Expr* expr_simple() {
        auto start = current().range.start;
        auto e = expr_simple_();
        auto end = previous().range.end;
        visit(e, {start, end});
        return e;
    }

    const std::vector<TokenType> allowedExprStarts{ID, INT, '{'};

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
                break;
            }
            auto e = expr();
            auto end = previous().range.end;
            addAttr(attrs, *path, e, {start, end});
            if (!expect(';')) {
                break;
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

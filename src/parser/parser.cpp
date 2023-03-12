#include "parser.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/parser-tab.hh>
#include <nix/symbol-table.hh>
#include <nix/util.hh>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string_view>
#include "common/position.h"
#include "parser/tokenizer.h"
#include "schema/schema.h"

struct Parser {
    nix::EvalState& state;
    Analysis& analysis;
    std::string source;
    Position targetPos;

    Tokenizer tokenizer = Tokenizer{state, analysis.path, source};

    Token current() { return tokenizer.current; }

    Token consume() {
        auto result = current();
        tokenizer.advance();
        return result;
    }

    std::optional<Token> allow(TokenType type) {
        if (current().type == type) {
            return current();
        }
        return {};
    }

    std::optional<Token> accept(TokenType type) {
        if (allow(type)) {
            return consume();
        }
        return {};
    }

    std::optional<Token> expect(TokenType type) {
        if (allow(type)) {
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
        analysis.parseErrors.push_back({msg, range});
    }

    nix::Expr* expr() {
        auto start = current().range.start;
        auto e = expr_simple();
        auto end = current().range.start;
        // if (Range{start, end}.contains(targetPos)) {
        // analysis.exprPath.push_back({e});
        // }
        return e;
    }

    nix::Expr* expr_simple() {
        // ID
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
        // '{' binds '}'
        if (accept('{')) {
            auto e = binds();
            expect('}');
            return e;
            // }
        }
        return missing();
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
            auto end = current().range.start;
            addAttr(attrs, *path, e, {start, end});
            if (!expect(';')) {
                break;
            }
        }
        return attrs;
    }

    nix::AttrPath* attrPath() {
        auto path = new nix::AttrPath;

        while (allow(ID) || allow(OR_KW)) {
            if (auto id = accept(ID)) {
                std::string_view name = get<std::string>(id->val);
                path->push_back(state.symbols.create(name));
                if (!accept('.')) {
                    break;
                }
            } else if (accept(OR_KW)) {
                path->push_back(state.symbols.create("or"));
                if (!accept('.')) {
                    break;
                }
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
    analysis.exprPath.push_back(parser.expr());

    return analysis;
}

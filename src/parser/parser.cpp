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
#include "common/position.h"
#include "parser/tokenizer.h"
#include "schema/schema.h"

struct Parser {
    nix::EvalState& state;
    Analysis& analysis;
    const std::vector<Token>& tokens;
    Position targetPos;

    size_t nextTokenIndex = 0;
    nix::Symbol sNull = state.symbols.create("null");

    Token lookahead(int i) {
        int index = nextTokenIndex + i;
        if (index < 0 || index >= tokens.size()) {
            Position pos = tokens.back().range.start;
            return {YYEOF, 0, {pos, pos}};
        }
        return tokens[index];
    }

    Token consume() { return tokens[nextTokenIndex++]; }

    std::optional<Token> accept(TokenType type) {
        if (lookahead(0).type == type) {
            return consume();
        }
        return {};
    }

    nix::PosIdx posIdx(Position position) {
        return state.positions.add(
            {analysis.path, nix::foFile}, position.line + 1, position.col + 1
        );
    }

    nix::Expr* missing() { return new nix::ExprVar(sNull); }

    nix::Expr* expr() {
        auto start = lookahead(0).range.start;
        auto e = expr_simple();
        auto end = lookahead(-1).range.end;
        if (Range{start, end}.contains(targetPos)) {
            analysis.exprPath.push_back(e);
        }
        return e;
    }

    nix::Expr* expr_simple() {
        // ID
        if (auto id = accept(ID)) {
            std::string_view name = id->val.id;
            if (name == "__curPos") {
                return new nix::ExprPos(posIdx(id->range.start));
            } else {
                return new nix::ExprVar(
                    posIdx(id->range.start), state.symbols.create(id->val.id)
                );
            }
        }
        consume();
        return missing();
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

    auto tokens = tokenize(state, path, source);

    Parser parser{state, analysis, tokens, targetPos};
    parser.expr();

    return analysis;
}

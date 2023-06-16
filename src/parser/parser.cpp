#include "parser.h"
#include <nix/attr-set.hh>
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/parser-tab.hh>
#include <nix/pos.hh>
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
#include "common/stringify.h"
#include "parser/tokenizer.h"
#include "schema/schema.h"

struct Parser {
    nix::EvalState& state;
    Analysis& analysis;
    Position targetPos;
    bool justReportedError = false;

    Tokenizer tokenizer = Tokenizer{state, analysis.path, analysis.source};
    // tokens[0] == previous()
    // tokens[1] == current() == lookahead(0)
    // tokens[2...] == lookahead(1), lookahead(2), ...
    // tokens.size() >= 4 but may be more
    std::deque<Token> tokens;

    Parser(nix::EvalState& state, Analysis& analysis, Position targetPos)
        : state(state),
          analysis(analysis),
          targetPos(targetPos),
          tokenizer(state, analysis.path, analysis.source),
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
        return position.posIdx(state, analysis.path);
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
        e->startPos = posIdx(range.start);
        e->endPos = posIdx(range.end);
        // extended because for
        //     aaa^
        // we want aaa to be in the exprPath
        auto r = range.extended();
        // because we want
        //    with pkgs;    ^
        // to parse properly
        if (current().type == YYEOF) {
            r.end = current().range.start;
        }
        if (r.contains(targetPos)) {
            analysis.exprPath.push_back({e});
        }
    }

    // GRAMMAR

    const std::vector<TokenType> allowedKeywordExprStarts{
        ASSERT,
        WITH,
        LET,
    };
    const std::vector<TokenType> allowedExprStarts{
        ASSERT, WITH, LET, IF,  ID,  OR_KW, INT, FLOAT, '"',   IND_STRING_OPEN,
        REC,    '(',  '{', '[', '+', '-',   '!', PATH,  HPATH, SPATH,
    };

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
            lookaheadMatches({'{', '}', '@'}) ||
            lookaheadMatches({'{', ID, '}', ':'}) ||
            lookaheadMatches({'{', ID, '}', '@'}) ||
            lookaheadMatches({'{', ELLIPSIS})) {
            auto start = current().range.start;
            accept('{');
            auto fs = formals();
            expect('}');
            nix::Symbol arg;
            if (accept('@')) {
                arg = state.symbols.create(get<std::string>(expect(ID)->val));
                if (previous().range.extended().contains(targetPos)) {
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
            return keyword_expression(true, &Parser::expr);
        }
        return expr_if();
    }

    // these are assert, with, let, if.
    // subExpr is used to parse sub expressions (not followed by ';').
    // for example in a list it is expr_select
    nix::Expr* keyword_expression(
        bool allowed,
        // pointer to member function
        // https://isocpp.org/wiki/faq/pointers-to-members#fnptr-vs-memfnptr-types
        nix::Expr* (Parser::*subExpr)()
    ) {
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
            auto body = (this->*subExpr)();
            result = new nix::ExprAssert(posIdx(start), cond, body);
        } else if (accept(WITH)) {
            auto attrs = expr();
            expect(';');
            auto body = (this->*subExpr)();
            result = new nix::ExprWith(posIdx(start), attrs, body);
        } else if (accept(LET)) {
            auto attrs = binds(false);
            expect(IN);
            auto body = (this->*subExpr)();
            result = new nix::ExprLet(attrs, body);
        }
        auto end = previous().range.end;
        visit(result, {start, end});
        return result;
    }

    struct BindingPower {
        int binding_power;
        enum {
            RIGHT,
            LEFT,
            NONE,
        } associativity;
    };

    BindingPower binding_power(TokenType type) {
        switch (type) {
            case IMPL:
                return {10, BindingPower::RIGHT};
            case OR:
                return {20, BindingPower::LEFT};
            case AND:
                return {30, BindingPower::LEFT};
            case EQ:
            case NEQ:
                return {40, BindingPower::NONE};
            case '<':
            case '>':
            case LEQ:
            case GEQ:
                return {50, BindingPower::NONE};
            case UPDATE:
                return {60, BindingPower::RIGHT};
            case '+':
                return {80, BindingPower::LEFT};
            // case '-':
            //     return {80, BindingPower::LEFT};
            case '*':
                return {90, BindingPower::LEFT};
            case CONCAT:
                return {100, BindingPower::RIGHT};
            case '?':
                return {110, BindingPower::NONE};
            default:
                return {-1, BindingPower::NONE};
        }
    }

    nix::Expr* expr_if() {
        if (accept(IF)) {
            auto cond = expr();
            expect(THEN);
            auto then = expr();
            expect(ELSE);
            auto else_ = expr();
            return new nix::ExprIf(nix::noPos, cond, then, else_);
        }
        return expr_op();
    }

    nix::Expr* expr_op(int min_binding_power = 0) {
        auto start = current().range.start;
        auto e = expr_app();
        bool nonassoc_encountered = false;

        while (binding_power(current().type).binding_power >= min_binding_power
        ) {
            auto op = consume();
            auto [bp, associativity] = binding_power(op.type);
            auto curPos = posIdx(op.range.start);

            if (associativity == BindingPower::NONE) {
                if (nonassoc_encountered) {
                    error(
                        "operator " + tokenName(op.type) +
                            " is not associative",
                        op.range
                    );
                }
                nonassoc_encountered = true;
            }

            if (op.type == '?') {
                e = new nix::ExprOpHasAttr(e, *attrPath());
                visit(e, {start, previous().range.end});
                continue;
            }

            auto rhs =
                expr_op(associativity == BindingPower::RIGHT ? bp : bp + 1);

            switch (op.type) {
                case EQ:
                    e = new nix::ExprOpEq(e, rhs);
                    break;
                case NEQ:
                    e = new nix::ExprOpNEq(e, rhs);
                    break;
                case '<':
                    e = new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__lessThan")),
                        {e, rhs}
                    );
                    break;
                case LEQ:
                    e = new nix::ExprOpNot(new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__lessThan")),
                        {rhs, e}
                    ));
                    break;
                case '>':
                    e = new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__lessThan")),
                        {rhs, e}
                    );
                    break;
                case GEQ:
                    e = new nix::ExprOpNot(new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__lessThan")),
                        {e, rhs}
                    ));
                    break;
                case AND:
                    e = new nix::ExprOpAnd(curPos, e, rhs);
                    break;
                case OR:
                    e = new nix::ExprOpOr(curPos, e, rhs);
                    break;
                case IMPL:
                    e = new nix::ExprOpImpl(curPos, e, rhs);
                    break;
                case UPDATE:
                    e = new nix::ExprOpUpdate(curPos, e, rhs);
                    break;
                case '-':
                    e = new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__sub")),
                        {e, rhs}
                    );
                    break;
                case '+':
                    e = new nix::ExprConcatStrings(
                        curPos,
                        false,
                        new std::vector<std::pair<nix::PosIdx, nix::Expr*>>(
                            {{posIdx(start), e}, {posIdx(op.range.end), rhs}}
                        )
                    );
                    break;
                case '*':
                    e = new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__mul")),
                        {e, rhs}
                    );
                    break;
                case '/':
                    e = new nix::ExprCall(
                        curPos,
                        new nix::ExprVar(state.symbols.create("__div")),
                        {e, rhs}
                    );
                    break;
                case CONCAT:
                    e = new nix::ExprOpConcatLists(curPos, e, rhs);
                    break;
            }
            visit(e, {start, previous().range.end});
        }

        return e;
    }

    nix::Expr* expr_app() {
        auto start = current().range.start;
        auto f = expr_select();
        if (allow(allowedExprStarts) && !allow({'+', '-'})) {
            auto call = new nix::ExprCall(posIdx(start), f, {});
            while (allow(allowedExprStarts) && !allow({'+', '-'})) {
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
            nix::Expr* def = nullptr;
            if (accept(OR_KW)) {
                def = expr_select();
            }
            auto select = new nix::ExprSelect(posIdx(start), e, *path, def);
            auto end = previous().range.end;
            visit(select, {start, end});
            return select;
        } else {
            return e;
        }
    }

    nix::Expr* expr_simple() {
        auto start = current().range.start;
        if (!allow(allowedExprStarts)) {
            auto missingStart = previous().range.end;
            while (!allow({';', '}', ']', ')', IN, YYEOF, IMPL})) {
                consume();
            }
            auto e = missing();
            Range missingRange{missingStart, current().range.start};
            error("expected expression", missingRange);
            visit(e, missingRange);
            return e;
        }
        // '!' expr_if
        if (accept('!')) {
            auto e = new nix::ExprOpNot(expr_op(70));
            visit(e, {start, previous().range.end});
            return e;
        }
        // '-' expr_if
        if (accept('-')) {
            auto e = new nix::ExprCall(
                posIdx(previous().range.start),
                new nix::ExprVar(state.symbols.create("__sub")),
                {new nix::ExprInt(0), expr_simple()}
            );
            visit(e, {start, previous().range.end});
            return e;
        }
        // ID
        if (auto id = accept(ID)) {
            std::string_view name = get<std::string>(id->val);
            if (name == "__curPos") {
                auto e = new nix::ExprPos(posIdx(id->range.start));
                visit(e, {start, previous().range.end});
                return e;
            } else {
                auto e = new nix::ExprVar(
                    posIdx(id->range.start), state.symbols.create(name)
                );
                visit(e, {start, previous().range.end});
                return e;
            }
        }
        if (accept(OR_KW)) {
            auto e = new nix::ExprVar(
                posIdx(previous().range.start), state.symbols.create("or")
            );
            visit(e, {start, previous().range.end});
            return e;
        }
        // INT
        if (auto token = accept(INT)) {
            auto e = new nix::ExprInt(get<nix::NixInt>(token->val));
            visit(e, {start, previous().range.end});
            return e;
        }
        // FLOAT
        if (auto token = accept(FLOAT)) {
            auto e = new nix::ExprFloat(get<nix::NixFloat>(token->val));
            visit(e, {start, previous().range.end});
            return e;
        }
        // '"' string_parts '"'
        if (accept('"')) {
            auto e = string_parts();
            expect('"');
            visit(e, {start, previous().range.end});
            return e;
        }
        // IND_STRING_OPEN ind_string_parts IND_STRING_CLOSE
        if (accept(IND_STRING_OPEN)) {
            auto parts = ind_string_parts();
            auto e = stripIndentation(
                posIdx(current().range.start), state.symbols, parts
            );
            expect(IND_STRING_CLOSE);
            visit(e, {start, previous().range.end});
            return e;
        }
        // path_start string_parts_interpolated PATH_END
        if (allow({PATH, HPATH})) {
            auto token = consume();
            auto start = token.range.start;
            auto p = get<std::string>(token.val);
            nix::Path path;
            if (token.type == HPATH) {
                // remove leading slash
                p.erase(0, 1);
                path = nix::getHome() + p;
            } else {
                // PATH
                path = nix::absPath(
                    get<std::string>(token.val), analysis.basePath
                );
                // add back trailing slash to first segment
                if (p.ends_with('/') && p.length() > 1) {
                    path += "/";
                }
            };
            nix::Expr* pathExpr = new nix::ExprPath(path);
            // if there are interpolated parts add them
            visit(pathExpr, {start, previous().range.end});
            if (auto sparts =
                    dynamic_cast<nix::ExprConcatStrings*>(string_parts())) {
                sparts->es->insert(
                    sparts->es->begin(), {posIdx(start), pathExpr}
                );
                visit(sparts, {start, previous().range.end});
                return sparts;
            }
            expect(PATH_END);
            return pathExpr;
        }
        if (auto spath = accept(SPATH)) {
            auto path = get<std::string>(spath->val);
            auto pathWithoutAngleBrackets = path.substr(1, path.length() - 2);
            auto e = new nix::ExprCall(
                posIdx(start),
                new nix::ExprVar(state.symbols.create("__findFile")),
                {new nix::ExprVar(state.symbols.create("__nixPath")),
                 new nix::ExprString(pathWithoutAngleBrackets)}
            );
            visit(e, previous().range);
            return e;
        }
        // '{' binds '}'
        if (accept('{')) {
            auto e = binds(true);
            expect('}');
            visit(e, {start, previous().range.end});
            return e;
        }
        // 'REC' '{' binds '}'
        if (accept(REC)) {
            expect('{');
            auto e = binds(true);
            expect('}');
            e->recursive = true;
            visit(e, {start, previous().range.end});
            return e;
        }
        // '[' expr_list ']'
        if (accept('[')) {
            auto e = new nix::ExprList;
            while (allow(allowedExprStarts)) {
                if (allow(allowedKeywordExprStarts)) {
                    e->elems.push_back(
                        keyword_expression(false, &Parser::expr_select)
                    );
                } else {
                    e->elems.push_back(expr_select());
                }
            }
            expect(']');
            visit(e, {start, previous().range.end});
            return e;
        }
        // '(' expr ')'
        if (accept('(')) {
            auto result = expr();
            expect(')');
            return result;
        }
        if (allow(allowedKeywordExprStarts)) {
            return keyword_expression(false, &Parser::expr_simple);
        }
        assert(false);
    }

    nix::Expr* string_parts() {
        auto parts = new std::vector<std::pair<nix::PosIdx, nix::Expr*>>;
        auto start = current().range.start;
        while (allow({STR, DOLLAR_CURLY})) {
            if (auto token = accept(STR)) {
                auto s = get<std::string>(token->val);
                parts->push_back(
                    {posIdx(token->range.start), new nix::ExprString(s)}
                );
            } else {
                accept(DOLLAR_CURLY);
                auto start = current().range.start;
                auto e = expr();
                expect('}');
                parts->push_back({posIdx(start), e});
            }
        }
        if (parts->empty()) {
            return new nix::ExprString("");
        }
        if (parts->size() == 1 &&
            dynamic_cast<nix::ExprString*>((*parts)[0].second)) {
            return (*parts)[0].second;
        }
        return new nix::ExprConcatStrings(posIdx(start), true, parts);
    }

    std::vector<std::pair<nix::PosIdx, std::variant<nix::Expr*, NAStringToken>>>
    ind_string_parts() {
        std::vector<
            std::pair<nix::PosIdx, std::variant<nix::Expr*, NAStringToken>>>
            parts;
        while (true) {
            if (auto s = accept(IND_STR)) {
                parts.push_back(
                    {posIdx(previous().range.end),
                     std::get<NAStringToken>(s->val)}
                );
            } else if (accept(DOLLAR_CURLY)) {
                accept(DOLLAR_CURLY);
                auto start = current().range.start;
                auto e = expr();
                expect('}');
                parts.push_back({posIdx(start), e});
            } else {
                return parts;
            }
        }
    }

    nix::Expr* stripIndentation(
        const nix::PosIdx pos,
        nix::SymbolTable& symbols,
        std::vector<
            std::pair<nix::PosIdx, std::variant<nix::Expr*, NAStringToken>>>& es
    ) {
        if (es.empty())
            return new nix::ExprString("");

        /* Figure out the minimum indentation.  Note that by design
           whitespace-only final lines are not taken into account.  (So
           the " " in "\n ''" is ignored, but the " " in "\n foo''" is.) */
        bool atStartOfLine =
            true; /* = seen only whitespace in the current line */
        size_t minIndent = 1000000;
        size_t curIndent = 0;
        for (auto& [i_pos, i] : es) {
            auto* str = std::get_if<NAStringToken>(&i);
            if (!str || !str->hasIndentation) {
                /* Anti-quotations and escaped characters end the current
                 * start-of-line whitespace. */
                if (atStartOfLine) {
                    atStartOfLine = false;
                    if (curIndent < minIndent)
                        minIndent = curIndent;
                }
                continue;
            }
            for (size_t j = 0; j < str->s.length(); ++j) {
                if (atStartOfLine) {
                    if (str->s[j] == ' ')
                        curIndent++;
                    else if (str->s[j] == '\n') {
                        /* Empty line, doesn't influence minimum
                           indentation. */
                        curIndent = 0;
                    } else {
                        atStartOfLine = false;
                        if (curIndent < minIndent)
                            minIndent = curIndent;
                    }
                } else if (str->s[j] == '\n') {
                    atStartOfLine = true;
                    curIndent = 0;
                }
            }
        }

        /* Strip spaces from each line. */
        auto* es2 = new std::vector<std::pair<nix::PosIdx, nix::Expr*>>;
        atStartOfLine = true;
        size_t curDropped = 0;
        size_t n = es.size();
        auto i = es.begin();
        const auto trimExpr = [&](nix::Expr* e) {
            atStartOfLine = false;
            curDropped = 0;
            es2->emplace_back(i->first, e);
        };
        const auto trimString = [&](const NAStringToken& t) {
            std::string s2;
            for (size_t j = 0; j < t.s.length(); ++j) {
                if (atStartOfLine) {
                    if (t.s[j] == ' ') {
                        if (curDropped++ >= minIndent)
                            s2 += t.s[j];
                    } else if (t.s[j] == '\n') {
                        curDropped = 0;
                        s2 += t.s[j];
                    } else {
                        atStartOfLine = false;
                        curDropped = 0;
                        s2 += t.s[j];
                    }
                } else {
                    s2 += t.s[j];
                    if (t.s[j] == '\n')
                        atStartOfLine = true;
                }
            }

            /* Remove the last line if it is empty and consists only of
               spaces. */
            if (n == 1) {
                std::string::size_type p = s2.find_last_of('\n');
                if (p != std::string::npos &&
                    s2.find_first_not_of(' ', p + 1) == std::string::npos)
                    s2 = std::string(s2, 0, p + 1);
            }

            es2->emplace_back(i->first, new nix::ExprString(s2));
        };
        for (; i != es.end(); ++i, --n) {
            std::visit(nix::overloaded{trimExpr, trimString}, i->second);
        }

        /* If this is a single string, then don't do a concatenation. */
        return es2->size() == 1 &&
                       dynamic_cast<nix::ExprString*>((*es2)[0].second)
                   ? (*es2)[0].second
                   : new nix::ExprConcatStrings(pos, true, es2);
    }

    // copy+paste from parser.y
    void addAttr(
        nix::ExprAttrs* attrs,
        nix::AttrPath& attrPath,
        nix::Expr* e,
        Range attrPathRange,
        Range exprRange
    ) {
        const nix::PosIdx pos = posIdx(attrPathRange.start);
        nix::AttrPath::iterator i;
        // All attrpaths have at least one attr
        assert(!attrPath.empty());
        // Checking attrPath validity.
        // ===========================
        // make sure to visit nested exprs in the correct order
        std::vector<nix::Expr*> nestedStack;
        for (i = attrPath.begin(); i + 1 < attrPath.end(); i++) {
            if (i->symbol) {
                auto j = attrs->attrs.find(i->symbol);
                if (j != attrs->attrs.end()) {
                    if (!j->second.inherited) {
                        auto attrs2 =
                            dynamic_cast<nix::ExprAttrs*>(j->second.e);
                        if (!attrs2) {
                            error("duplicate attr", attrPathRange);
                            // dupAttr(state, attrPath, start, end,
                            // j->second.pos);
                            return;
                        }
                        attrs = attrs2;
                    } else {
                        error("duplicate attr", attrPathRange);
                        // dupAttr(state, attrPath, start, end,
                        // j->second.pos);
                        return;
                    }
                } else {
                    auto nested = new nix::ExprAttrs;
                    attrs->attrs[i->symbol] =
                        nix::ExprAttrs::AttrDef(nested, pos);
                    nestedStack.push_back(nested);
                    attrs = nested;
                }
            } else {
                auto nested = new nix::ExprAttrs;
                attrs->dynamicAttrs.push_back(
                    nix::ExprAttrs::DynamicAttrDef(i->expr, nested, pos)
                );
                nestedStack.push_back(nested);
                attrs = nested;
            }
        }
        for (int i = nestedStack.size() - 1; i >= 0; i--) {
            visit(nestedStack[i], exprRange);
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
                            error("duplicate attr", attrPathRange);
                            // dupAttr(state, ad.first, start, end,
                            // ad.second.pos);
                            return;
                        }
                        jAttrs->attrs.emplace(ad.first, ad.second);
                    }
                } else {
                    error("duplicate attr", attrPathRange);
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

    nix::ExprAttrs* binds(bool allowDynamic) {
        auto attrs = new nix::ExprAttrs(posIdx(current().range.start));
        while (allow({INHERIT, ID, DOLLAR_CURLY, '"'})) {
            auto start = current().range.start;
            if (accept(INHERIT)) {
                // inherited
                std::optional<nix::Expr*> inheritFrom = {};
                if (accept('(')) {
                    inheritFrom = expr();
                    expect(')');
                }
                bool foundInherit = false;
                while (auto id = accept(ID)) {
                    foundInherit = true;
                    auto symbol =
                        state.symbols.create(get<std::string>(id->val));
                    if (id->range.extended().contains(targetPos)) {
                        analysis.inherit = {{symbol, inheritFrom}};
                    }
                    if (attrs->attrs.find(symbol) != attrs->attrs.end()) {
                        error("duplicate attr", id->range);
                        continue;
                    }
                    auto pos = posIdx(id->range.start);
                    nix::Expr* def =
                        inheritFrom ? (nix::Expr*)new nix::ExprSelect(
                                          pos, *inheritFrom, symbol
                                      )
                                    : (nix::Expr*)new nix::ExprVar(pos, symbol);
                    attrs->attrs.emplace(
                        symbol, nix::ExprAttrs::AttrDef(def, pos, !inheritFrom)
                    );
                }
                if (Range{previous().range.end, current().range.start}
                        .extended()
                        .contains(targetPos)) {
                    analysis.inherit = {
                        {state.symbols.create(""), inheritFrom}};
                }
                if (!foundInherit && inheritFrom.has_value()) {
                    // if we don't do this, then for
                    // {
                    //    inherit (e) ;
                    // }
                    // bindVars will never happen on e and hover and
                    // autocomplete wont work (throwing undef var error)
                    auto symbol = state.symbols.create("");
                    nix::Expr* def =
                        new nix::ExprSelect(nix::noPos, *inheritFrom, symbol);
                    attrs->attrs.emplace(
                        symbol,
                        nix::ExprAttrs::AttrDef(def, nix::noPos, !inheritFrom)
                    );
                }
            } else {
                // not inherited
                auto attrPathStart = current().range.start;
                auto path = attrPath();
                bool isDynamic = false;
                for (auto el : *path) {
                    if (!el.symbol) {
                        isDynamic = true;
                        break;
                    }
                }
                auto attrPathEnd = previous().range.end;
                if (!expect('=')) {
                    continue;
                }
                auto subExprStart = current().range.start;
                auto e = expr();
                auto end = previous().range.end;
                if (isDynamic && !allowDynamic) {
                    error(
                        "dynamic attrs are not allowed in let",
                        {attrPathStart, attrPathEnd}
                    );
                } else {
                    addAttr(
                        attrs,
                        *path,
                        e,
                        {attrPathStart, attrPathEnd},
                        {subExprStart, end}
                    );
                }
            }
            if (accept(',')) {
                error("expected ';', got ','", previous().range);
                continue;
            }
            if (!expect(';')) {
                continue;
            }
        }
        return attrs;
    }

    nix::AttrPath* attrPath() {
        auto path = new nix::AttrPath;

        while (true) {
            Position start;
            if (path->size() > 0) {
                start = previous().range.end;
            } else {
                start = current().range.start;
            }
            bool dynamic = false;
            if (auto id = accept(ID)) {
                auto name = get<std::string>(id->val);
                path->push_back(state.symbols.create(name));
            } else if (accept(OR_KW)) {
                path->push_back(state.symbols.create("or"));
            } else if (accept(DOLLAR_CURLY)) {
                path->push_back(expr());
                dynamic = true;
                expect('}');
            } else if (accept('"')) {
                auto e = string_parts();
                expect('"');
                auto str = dynamic_cast<nix::ExprString*>(e);
                if (str) {
                    path->push_back(state.symbols.create(str->s));
                    delete str;
                } else {
                    path->push_back(e);
                    dynamic = true;
                }
            } else {
                auto dotPosition = previous().range.start;
                auto nextTokenPosition = current().range.start;
                path->push_back(state.symbols.create(""));
                Range missingIdRange{dotPosition, nextTokenPosition};
                error("expected ID", missingIdRange);
                while (!allow({'=', ';', '}', YYEOF})) {
                    consume();
                }
                if (missingIdRange.extended().contains(targetPos)) {
                    analysis.attr = {path->size() - 1, path};
                }
                break;
            }
            auto end = previous().range.end;
            if (!dynamic && Range{start, end}.extended().contains(targetPos)) {
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

        bool operator<(const ParserFormal& other) {
            if (formal.name == other.formal.name) {
                return range.start < other.range.start;
            }
            return formal.name < other.formal.name;
        }
    };

    struct ParserFormals {
        std::vector<ParserFormal> formals;
        bool ellipsis = false;
    };

    ParserFormals formals() {
        ParserFormals result;
        while (allow({ELLIPSIS, ID})) {
            if (accept(ELLIPSIS)) {
                result.ellipsis = true;
            } else {
                auto id = accept(ID);

                auto name = state.symbols.create(get<std::string>(id->val));
                nix::Formal formal{posIdx(id->range.start), name, nullptr};
                if (accept('?')) {
                    formal.def = expr();
                }
                if (id->range.extended().contains(targetPos)) {
                    analysis.formal = formal;
                }
                result.formals.push_back({formal, id->range});
            }
            if (!allow('}')) {
                expect(',');
            }
        }
        return result;
    }

    nix::Formals* to_formals(ParserFormals& formals, nix::Symbol arg) {
        nix::Formals result;
        result.ellipsis = formals.ellipsis;

        if (formals.formals.empty()) {
            return new nix::Formals(std::move(result));
        }

        std::sort(formals.formals.begin(), formals.formals.end());

        for (auto [formal, range] : formals.formals) {
            if (!result.formals.empty() &&
                (formal.name == result.formals.back().name || formal.name == arg
                )) {
                error("duplicate formal function argument", range);
            } else {
                result.formals.push_back(formal);
            }
        }

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
    analysis.path = path;
    analysis.basePath = basePath;
    analysis.source = source;

    Parser parser{state, analysis, targetPos};
    auto e = parser.expr();
    if (analysis.exprPath.empty()) {
        analysis.exprPath.push_back(e);
    }
    analysis.exprPath.back().e->bindVars(state, state.staticBaseEnv);

    return analysis;
}

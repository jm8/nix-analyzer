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
            {analysis.path, nix::foFile},
            position.line + 1,
            position.character + 1
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

    const std::vector<TokenType> allowedKeywordExprStarts{
        ASSERT,
        WITH,
        LET,
        // IF
    };
    const std::vector<TokenType> allowedExprStarts{
        ASSERT,
        WITH,
        LET,
        // IF,
        ID,
        OR_KW,
        INT,
        FLOAT,
        '"',
        // IND_STRING_OPEN,
        REC,
        '{',
        '[',
        '-',
        '!',
        PATH,
        HPATH,
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
            return keyword_expression(true, &Parser::expr);
        }
        return expr_op();
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
            auto attrs = binds();
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

        if (allow(allowedExprStarts)) {
            auto call = new nix::ExprCall(posIdx(start), f, {});
            while (allow(allowedExprStarts)) {
                // we want
                // { a = a b c d.e.f = 2; }
                // to be parsed as
                // { a = a b c; d.e.f = 2; }
                // check if looking at potential attrpath
                if (lookaheadBind())
                    break;
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
        auto e = expr_simple_();
        auto end = previous().range.end;
        visit(e, {start, end});
        return e;
    }

    nix::Expr* expr_simple_() {
        if (!allow(allowedExprStarts)) {
            error("expected expression", current().range);
            while (!allow({';', '}',    ']', ')', IN,     YYEOF, IMPL,
                           OR,  AND,    EQ,  NEQ, '<',    '>',   LEQ,
                           GEQ, UPDATE, '+', '*', CONCAT, '?'})) {
                consume();
            }
            return missing();
        }
        // '!' expr_op
        if (accept('!')) {
            return new nix::ExprOpNot(expr_op(70));
        }
        // '-' expr_op
        if (accept('-')) {
            return new nix::ExprCall(
                posIdx(previous().range.start),
                new nix::ExprVar(state.symbols.create("__sub")),
                {new nix::ExprInt(0), expr_simple()}
            );
        }
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
        if (accept(OR_KW)) {
            return new nix::ExprVar(
                posIdx(previous().range.start), state.symbols.create("or")
            );
        }
        // INT
        if (auto token = accept(INT)) {
            return new nix::ExprInt(get<nix::NixInt>(token->val));
        }
        // FLOAT
        if (auto token = accept(FLOAT)) {
            return new nix::ExprFloat(get<nix::NixFloat>(token->val));
        }
        // '"' string_parts '"'
        if (accept('"')) {
            auto e = string_parts();
            expect('"');
            return e;
        }
        // path_start string_parts_interpolated PATH_END
        if (allow({PATH, HPATH})) {
            auto token = consume();
            auto start = token.range.start;
            nix::Path path;
            if (token.type == HPATH) {
                auto p = get<std::string>(token.val);
                // remove leading and trailing slash
                p = p.substr(1, p.length() - 1);
                path = nix::getHome() + p;
            } else {
                // PATH
                path = nix::absPath(
                    get<std::string>(token.val), analysis.basePath
                );
            };
            if (path.ends_with('/') && path.length() > 1) {
                path += "/";
            }
            nix::Expr* pathExpr = new nix::ExprPath(path);
            // if there are interpolated parts add them
            // if (auto sparts =
            //         dynamic_cast<nix::ExprConcatStrings*>(string_parts()))
            // {
            //     sparts->es->insert(
            //         sparts->es->begin(), {posIdx(start), pathExpr}
            //     );
            //     return sparts;
            // }
            expect(PATH_END);
            return pathExpr;
        }
        // '{' binds '}'
        if (accept('{')) {
            auto e = binds();
            expect('}');
            return e;
        }
        // 'REC' '{' binds '}'
        if (accept(REC)) {
            expect('{');
            auto e = binds();
            expect('}');
            e->recursive = true;
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
            return e;
        }
        if (allow(allowedKeywordExprStarts)) {
            return keyword_expression(false, &Parser::expr_simple);
        }
        std::cerr << tokenName(current().type) << "\n";
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

    bool lookaheadBind() {
        size_t i = 0;
        if (allow({IN, '}'}))
            return false;
        while (lookahead(i).type == ID || lookahead(i).type == OR_KW) {
            if (lookahead(i + 1).type == '.') {
                i += 2;
            } else {
                i += 1;
                break;
            }
        }
        if (lookahead(i).type == '=' || lookahead(i).type == IN ||
            lookahead(i).type == '}') {
            return true;
        }
        return false;
    }

    nix::ExprAttrs* binds() {
        auto attrs = new nix::ExprAttrs;
        while (lookaheadBind()) {
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
                if (id->range.contains(targetPos)) {
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
    analysis.basePath = basePath;

    Parser parser{state, analysis, source, targetPos};
    auto e = parser.expr();
    if (analysis.exprPath.empty()) {
        analysis.exprPath.push_back(e);
    }

    return analysis;
}

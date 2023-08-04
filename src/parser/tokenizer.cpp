#include "tokenizer.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/lexer-tab.hh>
#include <nix/parser-tab.hh>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "common/position.h"

Tokenizer::Tokenizer(
    nix::EvalState& state,
    std::string path,
    std::string source_
)
    : data(state, {path, nix::foFile}), source(source_), lastEnd() {
    source.append("\0\0", 2);

    yylex_init(&scanner);
    yy_scan_buffer(source.data(), source.length(), scanner);
}

Token Tokenizer::advance() {
    if (done)
        return {YYEOF, {}, {{lastEnd.line + 1, 0}, {lastEnd.line + 1, 0}}};
    Token token;
    token.type =
        static_cast<TokenType>(yylex(&yylval, &yylloc, scanner, &data));
    if (token.type == 0) {
        done = true;
        return {YYEOF, {}, {{lastEnd.line + 1, 0}, {lastEnd.line + 1, 0}}};
    }
    if (token.type == ID) {
        token.val = std::string{std::string_view{yylval.id}};
    } else if (token.type == STR) {
        token.val = std::string{std::string_view{yylval.str}};
    } else if (token.type == IND_STR) {
        token.val = NAStringToken{
            std::string{std::string_view{yylval.str}},
            yylval.str.hasIndentation};
    } else if (token.type == PATH || token.type == HPATH || token.type == SPATH) {
        token.val = std::string{std::string_view{yylval.path}};
    } else if (token.type == URI) {
        token.val = std::string{std::string_view{yylval.uri}};
    } else if (token.type == INT) {
        token.val = yylval.n;
    } else if (token.type == FLOAT) {
        token.val = yylval.nf;
    } else {
        token.val = {};
    }
    token.range.start = {
        static_cast<uint32_t>(yylloc.first_line - 1),
        static_cast<uint32_t>(yylloc.first_column - 1)};
    lastEnd = token.range.end = {
        static_cast<uint32_t>(yylloc.last_line - 1),
        static_cast<uint32_t>(yylloc.last_column - 1)};
    return token;
}

Tokenizer::~Tokenizer() {
    yylex_destroy(scanner);
}

std::string tokenName(TokenType type) {
    switch (type) {
        case -2:
            return "YYEMPTY";
        case 0:
            return "EOF";
        case 256:
            return "YYerror";
        case 257:
            return "YYUNDEF";
        case 258:
            return "ID";
        case 259:
            return "ATTRPATH";
        case 260:
            return "STR";
        case 261:
            return "IND_STR";
        case 262:
            return "INT";
        case 263:
            return "FLOAT";
        case 264:
            return "PATH";
        case 265:
            return "HPATH";
        case 266:
            return "SPATH";
        case 267:
            return "PATH_END";
        case 268:
            return "URI";
        case 269:
            return "IF";
        case 270:
            return "THEN";
        case 271:
            return "ELSE";
        case 272:
            return "ASSERT";
        case 273:
            return "WITH";
        case 274:
            return "LET";
        case 275:
            return "IN";
        case 276:
            return "REC";
        case 277:
            return "INHERIT";
        case 278:
            return "EQ";
        case 279:
            return "NEQ";
        case 280:
            return "AND";
        case 281:
            return "OR";
        case 282:
            return "IMPL";
        case 283:
            return "OR_KW";
        case 284:
            return "DOLLAR_CURLY";
        case 285:
            return "IND_STRING_OPEN";
        case 286:
            return "IND_STRING_CLOSE";
        case 287:
            return "ELLIPSIS";
        case 288:
            return "LEQ";
        case 289:
            return "GEQ";
        case 290:
            return "UPDATE";
        case 291:
            return "NOT";
        case 292:
            return "CONCAT";
        case 293:
            return "NEGATE";
        default:
            return std::string(1, type);
    }
}
#include "tokenizer.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/lexer-tab.hh>
#include <nix/parser-tab.hh>
#include <string>
#include <vector>

std::vector<Token> tokenize(
    nix::EvalState& state,
    std::string path,
    std::string source
) {
    nix::ParseData data(state, {path, nix::foFile});

    source.append("\0\0", 2);
    yyscan_t scanner;
    yylex_init(&scanner);
    yy_scan_buffer(source.data(), source.length(), scanner);

    TokenType type;
    YYLTYPE loc;
    YYSTYPE val;

    std::vector<Token> result;

    while ((type = static_cast<TokenType>(yylex(&val, &loc, scanner, &data)))) {
        result.push_back({type, val, loc});
    }

    yylex_destroy(scanner);
    return result;
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
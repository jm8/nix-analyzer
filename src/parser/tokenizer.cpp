#include "tokenizer.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/input-accessor.hh>
#include <nix/lexer-tab.hh>
#include <nix/parser-tab.hh>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "nixexpr.hh"
#include "position/position.h"

struct Tokenizer {
    nix::ParseData data;
    std::string source;
    yyscan_t scanner;
    YYLTYPE yylloc;
    YYSTYPE yylval;
    Position lastEnd;
    TokenIndex index;
    bool done = false;

    Tokenizer(
        nix::EvalState& state,
        nix::SourcePath path,
        nix::SourcePath basePath,
        std::string source
    );
    Token advance();
    ~Tokenizer();
};

Tokenizer::Tokenizer(
    nix::EvalState& state,
    nix::SourcePath path,
    nix::SourcePath basePath,
    std::string source_
)
    : data(nix::ParseData{
          state,
          state.symbols,
          nullptr,
          basePath,
          nix::Pos::Origin(path),
          {}}),
      source(source_),
      index(0),
      lastEnd() {
    source.append("\0\0", 2);

    yylex_init(&scanner);
    yy_scan_buffer(source.data(), source.length(), scanner);
}

Token Tokenizer::advance() {
    if (done)
        return {
            YYEOF, index, {}, {{lastEnd.line + 1, 0}, {lastEnd.line + 1, 0}}};
    Token token;
    token.index = index;
    index++;
    token.type =
        static_cast<TokenType>(yylex(&yylval, &yylloc, scanner, &data));
    if (token.type == 0) {
        done = true;
        return {
            YYEOF, index, {}, {{lastEnd.line + 1, 0}, {lastEnd.line + 1, 0}}};
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
        case YYEMPTY:
            return "YYEMPTY";
        case YYEOF:
            return "YYEOF";
        case YYerror:
            return "YYerror";
        case YYUNDEF:
            return "YYUNDEF";
        case ID:
            return "ID";
        case STR:
            return "STR";
        case IND_STR:
            return "IND_STR";
        case INT:
            return "INT";
        case FLOAT:
            return "FLOAT";
        case PATH:
            return "PATH";
        case HPATH:
            return "HPATH";
        case SPATH:
            return "SPATH";
        case PATH_END:
            return "PATH_END";
        case URI:
            return "URI";
        case IF:
            return "IF";
        case THEN:
            return "THEN";
        case ELSE:
            return "ELSE";
        case ASSERT:
            return "ASSERT";
        case WITH:
            return "WITH";
        case LET:
            return "LET";
        case IN:
            return "IN";
        case REC:
            return "REC";
        case INHERIT:
            return "INHERIT";
        case EQ:
            return "EQ";
        case NEQ:
            return "NEQ";
        case AND:
            return "AND";
        case OR:
            return "OR";
        case IMPL:
            return "IMPL";
        case OR_KW:
            return "OR_KW";
        case DOLLAR_CURLY:
            return "DOLLAR_CURLY";
        case IND_STRING_OPEN:
            return "IND_STRING_OPEN";
        case IND_STRING_CLOSE:
            return "IND_STRING_CLOSE";
        case ELLIPSIS:
            return "ELLIPSIS";
        case LEQ:
            return "LEQ";
        case GEQ:
            return "GEQ";
        case UPDATE:
            return "UPDATE";
        case NOT:
            return "NOT";
        case CONCAT:
            return "CONCAT";
        case NEGATE:
            return "NEGATE";
        default:
            return std::string(1, type);
    }
}

std::vector<Token> tokenize(
    nix::EvalState& state,
    nix::SourcePath path,
    nix::SourcePath basePath,
    std::string source
) {
    Tokenizer tokenizer(state, path, basePath, source);
    Token token = tokenizer.advance();
    std::vector<Token> tokens;
    while (token.type != YYEOF) {
        tokens.push_back(token);
        token = tokenizer.advance();
    }
    // push one EOF token
    tokens.push_back(token);
    return tokens;
}

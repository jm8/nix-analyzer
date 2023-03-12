#pragma once
#include "config.h"
#include <nix/parser-tab.hh>
// lexer-tab must be included after parser-tab
#include <nix/lexer-tab.hh>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "common/position.h"

YY_DECL;

using TokenType = int;

struct Token {
    TokenType type;
    std::variant<std::monostate, std::string> val;
    Range range;
};

struct Tokenizer {
    nix::ParseData data;
    std::string source;
    yyscan_t scanner;
    Token current;
    YYLTYPE yylloc;
    YYSTYPE yylval;

    Tokenizer(nix::EvalState& state, std::string path, std::string source);
    void advance();
    ~Tokenizer();
};

std::string tokenName(TokenType type);
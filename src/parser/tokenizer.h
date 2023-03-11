#pragma once
#include "config.h"
#include <nix/parser-tab.hh>
// lexer-tab must be included after parser-tab
#include <nix/lexer-tab.hh>
#include <string>
#include <vector>
#include "common/position.h"

YY_DECL;

using TokenType = int;

struct Token {
    TokenType type;
    YYSTYPE val;
    Range range;
};

std::string tokenName(TokenType type);

std::vector<Token> tokenize(
    nix::EvalState& state,
    std::string path,
    std::string source
);
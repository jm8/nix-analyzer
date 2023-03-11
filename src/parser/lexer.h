#pragma once
#include "config.h"
#include <nix/parser-tab.hh>
// lexer-tab must be included after parser-tab
#include <nix/lexer-tab.hh>

YY_DECL;

using TokenType = int;

struct Token {
    TokenType type;
    YYSTYPE val;
    YYLTYPE loc;
};
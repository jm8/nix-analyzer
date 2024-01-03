#pragma once
#include "na_config.h"
#include <nix/parser-tab.hh>
// lexer-tab must be included after parser-tab
#include <nix/lexer-tab.hh>
#include <nix/value.hh>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "position/position.h"

YY_DECL;

using TokenType = int;

struct NAStringToken {
    std::string s;
    bool hasIndentation;
};

struct Token {
    TokenType type;
    TokenIndex index;
    std::variant<
        std::monostate,
        std::string,
        nix::NixInt,
        nix::NixFloat,
        NAStringToken>
        val;
    Range range;
};

std::string tokenName(TokenType type);

std::vector<Token> tokenize(
    nix::EvalState& state,
    nix::SourcePath path,
    nix::SourcePath basePath,
    std::string source
);
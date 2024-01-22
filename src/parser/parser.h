#pragma once
#include "na_config.h"
#include <nix/nixexpr.hh>
#include <memory>
#include <string>
#include <vector>
#include "document/document.h"
#include "position/position.h"

struct ParseExprData {
    TokenRange range;
};

struct ParseResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> parseErrors;
    std::unordered_map<nix::Expr*, ParseExprData> exprData;
    nix::Expr* root;
};

ParseResult parse(
    nix::EvalState& state,
    nix::SourcePath path,
    std::string_view source
);
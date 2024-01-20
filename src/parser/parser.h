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
    std::shared_ptr<nix::StaticEnv> staticEnv;
};

struct ParseResult {
    std::unordered_map<nix::Expr*, ParseExprData> parseExprData;
};

Document parse(
    nix::EvalState& state,
    nix::SourcePath path,
    std::string_view source
);
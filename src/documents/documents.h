#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "fmt.hh"
#include "input-accessor.hh"
#include "nixexpr.hh"
#include "parser/tokenizer.h"
#include "position/position.h"

struct Diagnostic {
    std::string msg;
    Range range;
};

struct ExprData {
    TokenRange range;
    std::shared_ptr<nix::StaticEnv> staticEnv;
    std::optional<nix::Expr*> parent;
};

// Represents an immutable document at a particular time
struct Document {
    nix::SourcePath path;
    std::vector<Token> tokens;
    std::vector<Diagnostic> parseErrors;
    std::unordered_map<nix::Expr*, ExprData> exprData;
    nix::Expr* root;

    Range tokenRangeToRange(TokenRange tokenRange);
};

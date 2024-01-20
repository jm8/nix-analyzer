#pragma once
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "eval.hh"
#include "fmt.hh"
#include "parser/tokenizer.h"
#include "position/position.h"
#include "value.hh"

struct Diagnostic {
    std::string msg;
    Range range;
};

struct ExprData {
    TokenRange range;
    std::shared_ptr<nix::StaticEnv> staticEnv;
    std::optional<nix::Expr*> parent;
    std::optional<nix::Env*> env;
    std::optional<nix::Value*> v;
};

// Represents an immutable document at a particular time
struct Document {
    nix::EvalState& state;
    nix::SourcePath path;
    std::vector<Token> tokens;
    std::vector<Diagnostic> parseErrors;
    std::unordered_map<nix::Expr*, ExprData> exprData;
    nix::Expr* root;

    Range tokenRangeToRange(TokenRange tokenRange);

    std::shared_ptr<nix::StaticEnv> getStaticEnv(nix::Expr* e);
    nix::Env* getEnv(nix::Expr* e);
    nix::Value* thunk(nix::Expr* e, nix::Env* env);
};

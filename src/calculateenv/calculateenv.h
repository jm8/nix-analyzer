#pragma once
#include "config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include "common/analysis.h"

// returns the environment used to evaluate child
// (child must be a direct child of parent)
nix::Env* updateEnv(
    nix::EvalState& state,
    nix::Expr* parent,
    nix::Expr* child,
    nix::Env* up,
    std::optional<nix::Value*> lambdaArg
);

// initializes analysis.exprPath[].env
void calculateEnvs(nix::EvalState& state, Analysis& analysis);
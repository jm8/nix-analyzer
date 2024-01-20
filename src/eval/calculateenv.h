#pragma once
#include "na_config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>

// returns the environment used to evaluate child
// (child must be a direct child of parent)
nix::Env* updateEnv(
    nix::EvalState& state,
    nix::Expr* parent,
    nix::Expr* child,
    nix::Env* up
);

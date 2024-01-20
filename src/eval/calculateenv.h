#pragma once
#include "na_config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include "documents/documents.h"

// returns the environment used to evaluate child
// (child must be a direct child of parent)
nix::Env* updateEnv(
    Document& document,
    nix::Expr* parent,
    nix::Expr* child,
    nix::Env* up
);

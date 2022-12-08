#include "eval.hh"
#include "nixexpr.hh"
#include <iostream>

void debugExpr(nix::EvalState &state, std::ostream &s, nix::Expr *e,
               int indent = 0);
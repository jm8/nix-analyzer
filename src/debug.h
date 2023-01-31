#include <iostream>
#include "eval.hh"
#include "nixexpr.hh"

const char* exprTypeName(nix::Expr* e);

std::string stringifyValue(nix::EvalState& state, nix::Value& v);
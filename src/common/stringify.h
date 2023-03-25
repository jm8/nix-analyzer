#include "config.h"
#include <nix/nixexpr.hh>
#include <fstream>
#include <sstream>

const char* exprTypeName(nix::Expr* e);

std::string stringify(nix::EvalState& state, nix::Expr* e);
std::string stringify(nix::EvalState& state, nix::Value* v);
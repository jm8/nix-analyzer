#include "analysis.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <optional>

ExprPathItem::ExprPathItem(nix::Expr* e) : e(e) {}
ExprPathItem::ExprPathItem(nix::Expr* e, nix::Env* env) : e(e), env(env) {}
ExprPathItem::ExprPathItem(
    nix::Expr* e,
    nix::Env* env,
    std::optional<nix::Value*> lambdaArg
)
    : e(e), env(env), lambdaArg(lambdaArg) {}

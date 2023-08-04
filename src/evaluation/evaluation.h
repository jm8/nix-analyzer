#include "na_config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include "common/analysis.h"

nix::Value* evaluateWithDiagnostics(
    nix::EvalState& state,
    nix::Expr* e,
    nix::Env& env,
    std::vector<Diagnostic>& diagnostics
);
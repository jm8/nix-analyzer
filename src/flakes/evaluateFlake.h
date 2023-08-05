#include "na_config.h"
#include <nix/value.hh>
#include <string>
#include <string_view>
#include <vector>
#include "common/analysis.h"

void computeFlakeDiagnostics(
    nix::EvalState& state,
    std::string_view path,
    nix::Expr* flake,
    std::vector<Diagnostic>& diagnostics
);

nix::Value* getFlakeLambdaArg(nix::EvalState& state, std::string_view path);
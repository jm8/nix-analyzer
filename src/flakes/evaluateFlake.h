#include "na_config.h"
#include <nix/flake/flake.hh>
#include <nix/value.hh>
#include <string>
#include <string_view>
#include <vector>
#include "common/analysis.h"

nix::flake::FlakeInputs parseFlakeInputs(
    nix::EvalState& state,
    std::string_view path,
    nix::Expr* flake,
    std::vector<Diagnostic>& diagnostics
);

std::optional<std::string> lockFlake(
    nix::EvalState& state,
    nix::Expr* flakeExpr,
    std::string_view path
);

nix::Value* getFlakeLambdaArg(nix::EvalState& state, std::string_view lockFile);
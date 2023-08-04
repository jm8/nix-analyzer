#include "na_config.h"
#include <nix/value.hh>
#include <string>
#include <string_view>
#include <vector>
#include "common/analysis.h"

nix::Value* evaluateFlake(
    nix::EvalState& state,
    nix::Expr* flake,
    std::optional<std::string_view> lockFile,
    std::vector<Diagnostic>& diagnostics
);
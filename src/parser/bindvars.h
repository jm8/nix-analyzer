#include "na_config.h"
#include <memory>
#include <vector>
#include "document/document.h"
#include "eval.hh"
#include "nixexpr.hh"

void bindVars(
    nix::EvalState& state,
    Document& document,
    std::shared_ptr<nix::StaticEnv> staticEnv,
    nix::Expr* e,
    std::optional<nix::Expr*> parent = {}
);
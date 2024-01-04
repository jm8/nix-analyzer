#include "na_config.h"
#include <memory>
#include <vector>
#include "documents/documents.h"
#include "eval.hh"
#include "nixexpr.hh"

void bindVars(
    nix::EvalState& state,
    Document& document,
    std::shared_ptr<nix::StaticEnv> staticEnv,
    nix::Expr* e
);
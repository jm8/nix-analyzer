#include "na_config.h"
#include <iostream>
#include "canon-path.hh"
#include "eval.hh"
#include "input-accessor.hh"
#include "search-path.hh"
#include "shared.hh"
#include "store-api.hh"
#include "value.hh"

int main() {
    nix::initNix();
    nix::initGC();
    auto stateptr = std::allocate_shared<nix::EvalState>(
        traceable_allocator<nix::EvalState>(),
        nix::SearchPath{},
        nix::openStore(),
        nullptr
    );
    auto& state = *stateptr;
    nix::Value v;
    state.eval(
        state.parseExprFromString(
            "2 + 2", nix::SourcePath{nix::CanonPath{"/"}}
        ),
        v
    );
    std::cout << nix::printValue(state, v) << "\n";
}
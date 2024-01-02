#include "na_config.h"
#include <iostream>
#include "canon-path.hh"
#include "eval.hh"
#include "input-accessor.hh"
#include "parser/parser.h"
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
        parse(
            state,
            nix::SourcePath{nix::CanonPath{"/"}},
            nix::SourcePath{nix::CanonPath{"/"}},
            "2+2"
        ),
        v
    );
    std::cout << nix::printValue(state, v) << "\n";
}
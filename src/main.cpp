#include "na_config.h"
#include <iostream>
#include <semaphore>
#include "canon-path.hh"
#include "eval.hh"
#include "input-accessor.hh"
#include "nixexpr.hh"
#include "parser/parser.h"
#include "parser/tokenizer.h"
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
    auto document = parse(
        state,
        nix::SourcePath{state.rootFS, nix::CanonPath{"/"}},
        nix::SourcePath{state.rootFS, nix::CanonPath{"/"}},
        "let x = 4; in "
    );
    for (auto& token : document.tokens) {
        std::cout << token.index << " " << tokenName(token.type) << "\n";
    }
    for (auto& err : document.parseErrors) {
        std::cout << err.msg << " " << err.range << "\n";
    }
}

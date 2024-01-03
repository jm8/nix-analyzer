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
        "let x = 4; in x"
    );
    for (auto& token : document.tokens) {
        std::cout << token.index << " " << tokenName(token.type) << "\n";
    }
    auto root_data = document.exprData.at(document.root);
    std::cout << root_data.range.start << " .. " << root_data.range.end << "\n";

    auto var = dynamic_cast<nix::ExprLet*>(document.root)->body;
    auto var_data = document.exprData[var];
    std::cout << var_data.range.start << " .. " << var_data.range.end << "\n";
}
#include <nix/config.h>
#include <iostream>
#include <memory>
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include "calculate4.h"
#include "parser/parser.h"

int main() {
    nix::initNix();
    nix::initGC();
    // nix::initGC();
    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    const auto source = "2 + 2";
    auto parseResult = parse(*state, source, "", nix::absPath("."),
                             nix::Pos{source, nix::foString, 1, 1});

    for (auto e : parseResult.exprPath) {
        e->show(state->symbols, std::cout);
        std::cout << "\n";
    }
}
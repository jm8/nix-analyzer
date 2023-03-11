#include <nix/config.h>
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "parser/parser.h"

int main() {
    nix::initNix();
    nix::initGC();
    // // nix::initGC();
    auto state = new nix::EvalState(nix::Strings{}, nix::openStore());

    const auto source = "(import <nixpkgs> {}).pkgs.stdenv.mkDerivation {}";
    // std::cout << state->staticBaseEnv << "\n";
    auto analysis = parse(
        *state,
        source,
        "",
        nix::absPath("."),
        nix::Pos{source, nix::foString, 1, 48}
    );

    calculateEnvs(*state, analysis);

    getSchema(*state, analysis);

    for (auto e : analysis.exprPath) {
        e.e->show(state->symbols, std::cout);
        std::cout << "\n";
    }
    std::cout << "\n";
}
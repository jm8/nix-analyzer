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
#include <nix/value.hh>
#include <optional>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "parser/parser.h"

int main() {
    nix::initNix();
    nix::initGC();
    // nix::initGC();
    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    const auto source = "let a = b; b = 4; in a";
    auto parseResult = parse(*state, source, "", nix::absPath("."),
                             nix::Pos{source, nix::foString, 1, 22});

    auto lambdaArgs =
        std::vector<std::optional<nix::Value*>>(parseResult.exprPath.size());

    for (auto e : parseResult.exprPath) {
        e->show(state->symbols, std::cout);
        std::cout << "\n";
    }

    auto env = calculateEnv(*state, parseResult.exprPath, lambdaArgs);
    nix::Value v;
    parseResult.exprPath.front()->eval(*state, *env, v);
    v.print(state->symbols, std::cout);
    std::cout << "\n";
}
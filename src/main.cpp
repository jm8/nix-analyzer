#include "config.h"
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <iostream>
#include <memory>
#include "calculateenv/calculateenv.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"

int main() {
    nix::initNix();
    nix::initGC();

    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    // auto analysis = parse(*state, "{ x = y; a = b;}", "", "", {0, 0});
    // analysis.exprPath.back().e->show(state->symbols, std::cout);
    // std::cout << "\n";
    Tokenizer tokenizer{*state, "", "{ x = y; a = b;}"};
    for (int i = 0; i < 15; i++) {
        std::cout << tokenName(tokenizer.current.type) << "\n";
        tokenizer.advance();
    }
}
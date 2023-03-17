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

    auto source =
        "1 -> 2 || -3 && 4 == 5 || 6 < 7 || 8 // !9 + 10 * 11 ++ 12 13 ? a // "
        "1";
    state->parseExprFromString(source, "")->show(state->symbols, std::cout);
    std::cout << "\n";
}
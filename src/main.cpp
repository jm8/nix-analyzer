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
    // // nix::initGC();
    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    auto tokens = tokenize(*state, "", "abc\ndef");
    for (const auto& token : tokens) {
        std::cout << tokenName(token.type) << token.range << "\n";
    }
}
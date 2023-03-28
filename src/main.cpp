#include "config.h"
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <iostream>
#include <memory>
#include "calculateenv/calculateenv.h"
#include "lsp/jsonrpc.h"
#include "lsp/lspserver.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"

int main() {
    nix::initNix();
    nix::initGC();

    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    // LspServer server{*state};
    // server.run(std::cin, std::cout);

    auto e =
        state->parseExprFromString("/hello/${username}/whatever", "/base-path");
    e->show(state->symbols, std::cout);
    std::cout << "\n";
}
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

    Tokenizer tokenizer{*state, "", "{ x.y = }"};

    for (int i = 0; i < 10; i++) {
        auto token = tokenizer.advance();
        std::cout << tokenName(token.type) << " " << token.range << "\n";
    }
}
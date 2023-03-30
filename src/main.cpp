#include "config.h"
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <iostream>
#include <memory>
#include <thread>
#include "calculateenv/calculateenv.h"
#include "lsp/jsonrpc.h"
#include "lsp/lspserver.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"

int main() {
    nix::initGC();
    nix::initNix();

    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    LspServer server{{std::cin, std::cout}, *state};
    server.run();
}
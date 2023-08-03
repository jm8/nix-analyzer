#include "config.h"
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <gc/gc.h>
#include <iostream>
#include <memory>
#include <thread>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/stringify.h"
#include "lsp/jsonrpc.h"
#include "lsp/lspserver.h"
#include "parser/parser.h"
#include "parser/tokenizer.h"
#include "schema/schema.h"

int main() {
    std::cerr << "Welcome to nix-analyzer!\n";
    nix::initGC();
    nix::initNix();

    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    LspServer server{{std::cin, std::cout}, *state};
    server.run();
}
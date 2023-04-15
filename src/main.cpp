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

    // it crashes when the GC is enabled shrug
    GC_disable();

    LspServer server{{std::cin, std::cout}, *state};
    server.run();

    // auto source =
    //     "let pkgs = import "
    //     "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source "
    //     "{}; in pkgs._0verkill.override { }";

    // for (int i = 0; i < 20; i++) {
    //     std::cerr << i << "\n";
    //     auto analysis = parse(*state, source, "", "", {0, 101});
    //     analysis.exprPath.back().e->bindVars(*state, state->staticBaseEnv);
    //     calculateEnvs(*state, analysis);

    //     Schema s = getSchema(*state, analysis);
    //     std::cerr << stringify(*state, s.value) << "\n";
    //     std::cerr << "\n\n";
    // }
}
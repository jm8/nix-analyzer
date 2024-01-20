#include "na_config.h"
#include <iostream>
#include <memory>
#include "canon-path.hh"
#include "eval.hh"
#include "input-accessor.hh"
#include "nixexpr.hh"
#include "parser/parser.h"
#include "search-path.hh"
#include "shared.hh"
#include "store-api.hh"
#include "stringify/stringify.h"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

std::unique_ptr<nix::EvalState> stateptr;

nix::SourcePath path(std::string p) {
    auto& state = *stateptr;
    return nix::SourcePath{state.rootFS, nix::CanonPath{p}};
}

int main(int argc, char** argv) {
    nix::initGC();
    nix::initNix();

    stateptr =
        std::make_unique<nix::EvalState>(nix::SearchPath{}, nix::openStore());
    return Catch::Session().run(argc, argv);
}
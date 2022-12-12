#pragma once

#include "config.h"

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_allocator.h>
#include <gc/gc_cpp.h>
#endif

#include <iostream>
#include <memory>

#include "eval-inline.hh"
#include "eval.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "nixexpr.hh"
#include "shared.hh"

#include "store-api.hh"
#include "util.hh"

struct Analysis {
    std::vector<nix::Expr*> exprPath;
    // nix::StaticEnv staticEnv;
};

struct NixAnalyzer
#if HAVE_BOEHMGC
    : gc
#endif
{
    using Callback = std::function<void(nix::Expr*, nix::Pos, nix::Pos)>;

    std::unique_ptr<nix::EvalState> state;

    NixAnalyzer(const nix::Strings& searchPath, nix::ref<nix::Store> store);

    // defined in parser.y
    void parseWithCallback(std::string text,
                           nix::FileOrigin origin,
                           const nix::PathView path,
                           const nix::PathView basePath,
                           std::shared_ptr<nix::StaticEnv>& staticEnv,
                           Callback callback);

    Analysis analyzeAtPos(std::string source, nix::Path basePath, nix::Pos pos);

    std::vector<std::string> complete(std::vector<nix::Expr*> exprPath);

    // returns the env that sub would be evaluated in within super.
    // sub must be a direct child of super.
    nix::Env& updateEnv(nix::Expr* super, nix::Expr* sub, nix::Env& up);
};

int poscmp(nix::Pos a, nix::Pos b);
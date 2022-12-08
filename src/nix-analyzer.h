#pragma once

#include "config.h"

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_allocator.h>
#include <gc/gc_cpp.h>
#endif

#include <iostream>
#include <memory>

#include "common-eval-args.hh"
#include "derivations.hh"
#include "eval-inline.hh"
#include "eval.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "nixexpr.hh"
#include "shared.hh"

#include "store-api.hh"
#include "util.hh"

struct NixAnalyzer
#if HAVE_BOEHMGC
    : gc
#endif
{
    std::unique_ptr<nix::EvalState> state;

    const static int envSize = 32768;
    std::shared_ptr<nix::StaticEnv> staticEnv;

    nix::Env *env;

    NixAnalyzer(const nix::Strings &searchPath, nix::ref<nix::Store> store);

    nix::Expr *parseString(std::string s);

    void evalString(std::string s, nix::Value &v);

    void printValue(std::ostream &s, nix::Value &v);
};
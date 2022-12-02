#include "config.h"

#include <iostream>
#include <memory>

#include "common-eval-args.hh"
#include "derivations.hh"
#include "eval-inline.hh"
#include "eval.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "shared.hh"

#include "store-api.hh"
#include "util.hh"

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_cpp.h>
#endif

using namespace std;
using namespace nix;

struct NixAnalyzer
#if HAVE_BOEHMGC
    : gc
#endif
{
    unique_ptr<EvalState> state;

    const static int envSize = 32768;
    shared_ptr<StaticEnv> staticEnv;

    Env *env;

    NixAnalyzer(const Strings &searchPath, nix::ref<Store> store);

    Expr *parseString(string s);

    void evalString(string s, Value &v);

    void printValue(std::ostream &s, Value &v);
};

NixAnalyzer::NixAnalyzer(const Strings &searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)),
      staticEnv(new StaticEnv(false, state->staticBaseEnv.get())) {}

Expr *NixAnalyzer::parseString(string s) {
    return state->parseExprFromString(s, absPath("."), staticEnv);
}

void NixAnalyzer::evalString(string s, Value &v) {
    Expr *e = parseString(s);
    e->eval(*state, *env, v);
}

void NixAnalyzer::printValue(std::ostream &s, Value &v) {
    v.print(state->symbols, s);
}

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());
    Value v;
    analyzer->evalString("2 + 2", v);
    analyzer->printValue(cout, v);
    cout << endl;
}
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
#include "nixexpr.hh"
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

    void debugExpr(std::ostream &s, Expr *e, int indent = 0);
};

NixAnalyzer::NixAnalyzer(const Strings &searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)),
      staticEnv(new StaticEnv(false, state->staticBaseEnv.get())) {
}

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

bool poscmp(Pos a, Pos b) {
    if (a.line > b.line)
        return 1;
    if (a.line < b.line)
        return -1;
    if (a.column > b.column)
        return 1;
    if (a.column < b.column)
        return -1;
    return 0;
}

// wip
void evaluateSubexpression(EvalState &state, Env &env, Expr *expr,
                           Pos targetPos, Value &v) {
    if (auto *e = dynamic_cast<ExprAttrs *>(expr)) {
        // find expr with last position before target
        Expr *sub = nullptr;
        for (auto &[symbol, attrdef] : e->attrs) {
            if (!sub || poscmp(state.positions[attrdef.pos],
                               state.positions[sub->getPos()]) > 0) {
                sub = attrdef.e;
            }
        }
        return evaluateSubexpression(state, env, sub, targetPos, v);
    }
    return state.eval(expr, v);
}

// int main() {
//     initNix();
//     initGC();

//     Strings searchPath;
//     auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());
//     Expr *root = analyzer->parseString("{a = 123; b = 456;}");
//     ExprAttrs *e = dynamic_cast<ExprAttrs *>(root);
//     auto [symbol, attrdef] = *e->attrs.begin();
//     auto pos = analyzer->state->positions[attrdef.pos];
//     cout << pos << endl;
// }
#include "nix-analyzer.h"

using namespace std;
using namespace nix;

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

int poscmp(Pos a, Pos b) {
    if (a.line > b.line) {
        return 1;
    }
    if (a.line < b.line) {
        return -1;
    }
    if (a.column > b.column) {
        return 1;
    }
    if (a.column < b.column) {
        return -1;
    }
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
#include "nix-analyzer.h"
#include "debug.h"

#include "error.hh"
#include "nixexpr.hh"

using namespace std;
using namespace nix;

NixAnalyzer::NixAnalyzer(const Strings& searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)) {
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

Analysis NixAnalyzer::analyzeAtPos(string source,
                                   Path basePath,
                                   Pos targetPos) {
    vector<Expr*> exprPath;
    // StaticEnv* se = state->staticBaseEnv;
    parseWithCallback(source, nix::foString, "", basePath, state->staticBaseEnv,
                      [targetPos, &exprPath](Expr* e, Pos start, Pos end) {
                          if (start.origin != targetPos.origin ||
                              start.file != targetPos.file) {
                              return;
                          }

                          if (!(poscmp(start, targetPos) <= 0 &&
                                poscmp(targetPos, end) <= 0)) {
                              return;
                          }

                          exprPath.push_back(e);
                      });
    return {exprPath};
}

vector<string> NixAnalyzer::complete(vector<Expr*> exprPath) {
    vector<string> result;
    Env* env = &state->baseEnv;
    for (size_t i = exprPath.size() - 1; i >= 1; i--) {
        Expr* sub = exprPath[i - 1];
        Expr* super = exprPath[i];
        env = &updateEnv(super, sub, *env);
    }
    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        AttrPath path(select->attrPath.begin(), select->attrPath.end() - 1);
        ExprSelect prefix(select->pos, select->e, path, select->def);
        Value v;
        prefix.eval(*state, *env, v);

        if (v.type() == nAttrs) {
            for (auto attr : *v.attrs) {
                result.push_back(state->symbols[attr.name]);
            }
        }
    }
    if (auto var = dynamic_cast<ExprVar*>(exprPath.front())) {
        auto se = state->getStaticEnv(*var);
        cout << bool(se) << endl;
        // for (auto [symbol, displ] : se->vars) {
        // result.push_back(state->symbols[symbol]);
        // }
    }
    return result;
}

Env& NixAnalyzer::updateEnv(Expr* super, Expr* sub, Env& up) {
    if (auto let = dynamic_cast<ExprLet*>(super)) {
        Env& env2 = state->allocEnv(let->attrs->attrs.size());
        env2.up = &up;

        Displacement displ = 0;

        // if sub is an inherited let binding use the env from the parent.
        // if it is a non-inherited let binding or the body use env2.
        bool useSuperEnv = false;

        for (auto& [symbol, attrDef] : let->attrs->attrs) {
            env2.values[displ++] =
                attrDef.e->maybeThunk(*state, attrDef.inherited ? up : env2);
            if (attrDef.e == sub && attrDef.inherited)
                useSuperEnv = true;
        }
        return useSuperEnv ? up : env2;
    }
    return up;
}
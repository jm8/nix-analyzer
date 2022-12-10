#include "nix-analyzer.h"
#include "debug.h"
#include "error.hh"
#include "nixexpr.hh"

using namespace std;
using namespace nix;

NixAnalyzer::NixAnalyzer(const Strings& searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)),
      staticEnv(new StaticEnv(false, state->staticBaseEnv.get())) {
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

vector<Expr*> NixAnalyzer::parsePathToString(string source,
                                             Path basePath,
                                             Pos targetPos) {
    vector<Expr*> result;
    parseWithCallback(source, nix::foString, "", basePath, state->staticBaseEnv,
                      [targetPos, &result](Expr* e, Pos start, Pos end) {
                          if (start.origin != targetPos.origin ||
                              start.file != targetPos.file) {
                              cout << "wrong file: " << start.file
                                   << " expected " << targetPos.file << endl;
                              return;
                          }

                          if (!(poscmp(start, targetPos) <= 0 &&
                                poscmp(targetPos, end) <= 0)) {
                              return;
                          }

                          result.push_back(e);
                      });
    return result;
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
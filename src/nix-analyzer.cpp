#include "nix-analyzer.h"
#include "nixexpr.hh"

using namespace std;
using namespace nix;

NixAnalyzer::NixAnalyzer(const Strings& searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)),
      staticEnv(new StaticEnv(false, state->staticBaseEnv.get())) {
}

Expr* NixAnalyzer::parseString(string s) {
    return state->parseExprFromString(s, absPath("."), staticEnv);
}

void NixAnalyzer::evalString(string s, Value& v) {
    Expr* e = parseString(s);
    e->eval(*state, *env, v);
}

void NixAnalyzer::printValue(std::ostream& s, Value& v) {
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

vector<string> NixAnalyzer::complete(vector<Expr*> exprPath) {
    vector<string> result;
    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        AttrPath path(select->attrPath.begin(), select->attrPath.end() - 1);
        ExprSelect prefix(select->pos, select->e, path, select->def);
        Value v;
        state->eval(&prefix, v);

        if (v.type() == nAttrs) {
            for (auto attr : *v.attrs) {
                result.push_back(state->symbols[attr.name]);
            }
        }
    }
    return result;
}
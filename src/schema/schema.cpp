#include "schema/schema.h"
#include <filesystem>
#include <iostream>
#include <nix/attr-set.hh>
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include "common/analysis.h"

Schema::Schema() {}

Schema::Schema(nix::Value* v) : value(v) {}

void functionDescriptionValue(nix::Value& v,
                              nix::EvalState& state,
                              nix::Expr* fun,
                              nix::Env& env) {
    auto sFunction = state.symbols.create("function");
    auto sName = state.symbols.create("name");
    auto bindings = state.allocBindings(2);

    auto functionV = fun->maybeThunk(state, env);

    std::string name;
    if (auto var = dynamic_cast<nix::ExprVar*>(fun)) {
        name = state.symbols[var->name];
    } else if (auto select = dynamic_cast<nix::ExprSelect*>(fun)) {
        name = state.symbols[select->attrPath.back().symbol];
    }
    auto nameV = state.allocValue();
    nameV->mkString(name);
    bindings->push_back({sFunction, functionV});
    bindings->push_back({sName, nameV});

    v.mkAttrs(bindings);
}

Schema getSchema(nix::EvalState& state, const Analysis& analysis) {
    if (analysis.exprPath.empty())
        return {};
    const std::string getSchemaPath =
        "/home/josh/dev/nix-analyzer/src/schema/getSchema.nix";

    // auto vGetSchema = state.allocValue();
    // state.evalFile(getSchemaPath, *vGetSchema);

    auto vPath = state.allocValue();
    vPath->mkString(analysis.path);

    auto vFunctions = state.allocValue();
    // vFunctions->mkList(analysis.exprPath.size());
    state.mkList(*vFunctions, analysis.exprPath.size());

    nix::Value* v0 = state.allocValue();
    v0->mkNull();
    vFunctions->listElems()[0] = v0;

    for (int i = 1; i < analysis.exprPath.size(); i++) {
        nix::Value* v = state.allocValue();
        v->mkNull();
        if (auto call = dynamic_cast<nix::ExprCall*>(analysis.exprPath[i].e)) {
            if (analysis.exprPath[i - 1].e == call->fun) {
                v->mkNull();
            } else {
                functionDescriptionValue(*v, state, call->fun,
                                         *analysis.exprPath[i].env);
            }
        } else {
            v->mkNull();
        }
        vFunctions->listElems()[i] = v;
    }

    auto sPath = state.symbols.create("path");
    auto sFunctions = state.symbols.create("functions");
    auto bindings = state.allocBindings(2);
    bindings->push_back({sPath, vPath});
    bindings->push_back({sFunctions, vFunctions});
    auto vArg = state.allocValue();
    vArg->mkAttrs(bindings);

    return {};
}
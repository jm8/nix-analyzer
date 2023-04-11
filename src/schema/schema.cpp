#include "schema/schema.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/pos.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <iostream>
#include <vector>
#include "common/analysis.h"
#include "common/logging.h"

Schema::Schema(nix::EvalState& state) : value() {
    value = state.allocValue();
    value->mkAttrs(state.allocBindings(0));
}

Schema::Schema(nix::EvalState& state, nix::Value* v) : value(v) {}

nix::Value* _nixpkgsValue = nullptr;

nix::Value* nixpkgsValue(nix::EvalState& state) {
    if (_nixpkgsValue == nullptr) {
        auto pkgsFunction = state.allocValue();
        state.evalFile(
            "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source", *pkgsFunction
        );

        auto arg = state.allocValue();
        arg->mkAttrs(state.allocBindings(0));

        auto pkgs = state.allocValue();
        state.callFunction(*pkgsFunction, *arg, *pkgs, nix::noPos);

        _nixpkgsValue = pkgs;
    }
    return _nixpkgsValue;
}

nix::Value* functionDescriptionValue(
    nix::EvalState& state,
    nix::Expr* fun,
    nix::Env& env,
    nix::Value* pkgs
) {
    auto bindings = state.buildBindings(3);

    auto sName = state.symbols.create("name");
    std::string name;
    if (auto var = dynamic_cast<nix::ExprVar*>(fun)) {
        name = state.symbols[var->name];
    } else if (auto select = dynamic_cast<nix::ExprSelect*>(fun)) {
        name = state.symbols[select->attrPath.back().symbol];
    }
    auto nameV = state.allocValue();
    nameV->mkString(name);
    bindings.insert(sName, nameV);

    auto sFunction = state.symbols.create("function");
    nix::Value* functionV;
    try {
        functionV = fun->maybeThunk(state, env);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        functionV = state.allocValue();
        functionV->mkNull();
    }
    bindings.insert(sFunction, functionV);

    auto sPackages = state.symbols.create("pkgs");
    bindings.insert(sPackages, pkgs);

    auto v = state.allocValue();
    v->mkAttrs(bindings.finish());

    return v;
}

Schema getSchema(nix::EvalState& state, const Analysis& analysis) {
    if (analysis.exprPath.empty())
        return {state};

    const std::string getFunctionSchemaPath =
        "/home/josh/dev/nix-analyzer/src/schema/getFunctionSchema.nix";
    auto vGetFunctionSchema = state.allocValue();
    try {
        state.evalFile(getFunctionSchemaPath, *vGetFunctionSchema);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {state};
    }

    for (int i = 1; i < analysis.exprPath.size(); i++) {
        nix::ExprCall* call;
        std::cerr << "getSchema " << stringify(state, analysis.exprPath[i].e)
                  << "\n";
        if ((call = dynamic_cast<nix::ExprCall*>(analysis.exprPath[i].e)) &&
            analysis.exprPath[i - 1].e != call->fun) {
            try {
                auto pkgs = nixpkgsValue(state);
                auto vFunctionDescription = functionDescriptionValue(
                    state, call->fun, *analysis.exprPath[i].env, pkgs
                );
                std::cerr << "vGetFunctionSchema = "
                          << stringify(state, vGetFunctionSchema) << "\n";
                std::cerr << "vGetFunctionSchema->lambda.fun->body = "
                          << stringify(
                                 state, vGetFunctionSchema->lambda.fun->body
                             )
                          << "\n";
                auto vSchema = state.allocValue();
                state.callFunction(
                    *vGetFunctionSchema,
                    *vFunctionDescription,
                    *vSchema,
                    nix::noPos
                );
                return {state, vSchema};
            } catch (nix::Error& e) {
                REPORT_ERROR(e);
                return {state};
            }
        }
    }

    return {state};
}

std::vector<nix::Symbol> Schema::attrs(nix::EvalState& state) {
    try {
        state.forceAttrs(*value, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    std::vector<nix::Symbol> result;
    for (auto x : *value->attrs) {
        result.push_back(x.name);
    }
    return result;
}
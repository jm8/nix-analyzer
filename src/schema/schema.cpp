#include "schema/schema.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common/analysis.h"
#include "common/logging.h"

nix::Value* loadFile(nix::EvalState& state, std::string path) {
    auto v = state.allocValue();
    try {
        state.evalFile("/home/josh/dev/nix-analyzer/src/" + path, *v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        v->mkNull();
    }
    return v;
}

nix::Value* makeAttrs(
    nix::EvalState& state,
    std::vector<std::pair<std::string_view, nix::Value*>> binds
) {
    auto bindings = state.buildBindings(binds.size());
    for (auto [a, b] : binds) {
        bindings.insert(state.symbols.create(a), b);
    }
    auto result = state.allocValue();
    result->mkAttrs(bindings.finish());
    return result;
}

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

struct SchemaRoot {
    Schema schema;
    // index in ExprPath for this SchemaRoot
    size_t index;
};

SchemaRoot getSchemaRoot(nix::EvalState& state, const Analysis& analysis) {
    auto emptySchema = state.allocValue();
    emptySchema->mkAttrs(state.allocBindings(0));
    if (analysis.exprPath.empty())
        return {emptySchema, 0};

    auto vGetFunctionSchema = loadFile(state, "schema/getFunctionSchema.nix");

    for (size_t i = 1; i < analysis.exprPath.size(); i++) {
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
                auto vSchema = state.allocValue();
                state.callFunction(
                    *vGetFunctionSchema,
                    *vFunctionDescription,
                    *vSchema,
                    nix::noPos
                );
                return {vSchema, i};
            } catch (nix::Error& e) {
                REPORT_ERROR(e);
                return {emptySchema, 0};
            }
        }
    }

    const std::string getFileSchemaPath =
        "/home/josh/dev/nix-analyzer/src/schema/getFileSchema.nix";
    auto vGetFileSchema = state.allocValue();
    try {
        state.evalFile(getFileSchemaPath, *vGetFileSchema);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {emptySchema, 0};
    }

    auto vFileDescription = state.allocValue();

    auto bindings = state.buildBindings(2);

    bindings.insert(state.symbols.create("pkgs"), nixpkgsValue(state));

    auto vPath = state.allocValue();
    vPath->mkPath(analysis.path);
    bindings.insert(state.symbols.create("path"), vPath);

    vFileDescription->mkAttrs(bindings.finish());

    try {
        auto vSchema = state.allocValue();
        state.callFunction(
            *vGetFileSchema, *vFileDescription, *vSchema, nix::noPos
        );
        return {vSchema, analysis.exprPath.size() - 1};
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {emptySchema, 0};
    }

    return {emptySchema, 0};
}

Schema getSchema(nix::EvalState& state, const Analysis& analysis) {
    auto schemaRoot = getSchemaRoot(state, analysis);
    Schema current = schemaRoot.schema;
    for (int i = schemaRoot.index; i >= 1; i--) {
        auto parent = analysis.exprPath[i].e;
        auto child = analysis.exprPath[i - 1].e;
        if (auto attrs = dynamic_cast<nix::ExprAttrs*>(parent)) {
            std::optional<nix::Symbol> subname;
            for (auto [symbol, attrDef] : attrs->attrs) {
                if (!attrDef.inherited && attrDef.e == child) {
                    subname = symbol;
                }
            }
            if (!subname) {
                std::cerr
                    << "Failed to find the child in the parent attrs so don't "
                       "know the symbol\n";
                return {};
            }
            current = current.attrSubschema(state, *subname);
        }
    }
    return current;
}

std::vector<nix::Symbol> Schema::attrs(nix::EvalState& state) {
    try {
        state.forceAttrs(*value, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    if (value->attrs->get(state.symbols.create("_type"))) {
        return {};
    }
    std::vector<nix::Symbol> result;
    for (nix::Attr x : *value->attrs) {
        std::string_view s = state.symbols[x.name];
        if (s.starts_with("_"))
            continue;
        result.push_back(x.name);
    }
    return result;
}

Schema Schema::attrSubschema(nix::EvalState& state, nix::Symbol symbol) {
    auto vFunction = loadFile(state, "schema/getAttrSubschema.nix");
    auto vSymbol = state.allocValue();
    vSymbol->mkString(state.symbols[symbol]);
    auto vArg = makeAttrs(
        state,
        {
            {"pkgs", nixpkgsValue(state)},
            {"symbol", vSymbol},
            {"parent", value},
        }
    );
    auto vRes = state.allocValue();
    try {
        state.callFunction(*vFunction, *vArg, *vRes, nix::noPos);
    } catch (nix::Error& e) {
        vRes->mkAttrs(state.allocBindings(0));
        REPORT_ERROR(e);
    }

    return {vRes};
}
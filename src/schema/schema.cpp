#include "schema/schema.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>

#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common/analysis.h"
#include "common/evalutil.h"
#include "common/logging.h"
#include "hover/hover.h"

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

size_t fileRootIndex(const Analysis& analysis) {
    for (int i = analysis.exprPath.size() - 1; i >= 0; i--) {
        if (dynamic_cast<nix::ExprLambda*>(analysis.exprPath[i].e) ||
            dynamic_cast<nix::ExprWith*>(analysis.exprPath[i].e) ||
            dynamic_cast<nix::ExprLet*>(analysis.exprPath[i].e)) {
            continue;
        }
        return i;
    }
    return 0;
}

SchemaRoot getSchemaRoot(nix::EvalState& state, const Analysis& analysis) {
    auto emptySchema = state.allocValue();
    emptySchema->mkAttrs(state.allocBindings(0));
    if (analysis.exprPath.empty())
        return {emptySchema, 0};

    auto vGetFunctionSchema = loadFile(state, "schema/getFunctionSchema.nix");

    for (size_t i = 1; i < analysis.exprPath.size(); i++) {
        nix::ExprCall* call;
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

    auto vGetFileSchema = loadFile(state, "schema/getFileSchema.nix");
    auto vFileDescription = state.allocValue();

    auto bindings = state.buildBindings(2);

    bindings.insert(state.symbols.create("pkgs"), nixpkgsValue(state));

    auto vPath = state.allocValue();
    vPath->mkString(analysis.path);
    bindings.insert(state.symbols.create("path"), vPath);

    vFileDescription->mkAttrs(bindings.finish());

    try {
        auto vSchema = state.allocValue();
        state.callFunction(
            *vGetFileSchema, *vFileDescription, *vSchema, nix::noPos
        );
        return {vSchema, fileRootIndex(analysis)};
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
                auto emptySchema = state.allocValue();
                emptySchema->mkAttrs(state.allocBindings(0));
                return {emptySchema};
            }
            current = current.attrSubschema(state, *subname);
        } else if (auto lambda = dynamic_cast<nix::ExprAttrs*>(child)) {
            current = current.functionSubschema(state);
        }
    }
    return current;
}

static nix::Value* attrSubschemas(nix::EvalState& state, Schema parent) {
    auto vRes = state.allocValue();
    try {
        auto vFunction = loadFile(state, "schema/getAttrSubschemas.nix");
        auto vArg = makeAttrs(
            state,
            {
                {"pkgs", nixpkgsValue(state)},
                {"parent", parent.value},
            }
        );
        state.callFunction(*vFunction, *vArg, *vRes, nix::noPos);
        state.forceAttrs(*vRes, nix::noPos);
    } catch (nix::Error& e) {
        vRes->mkAttrs(state.allocBindings(0));
        REPORT_ERROR(e);
    }

    return vRes;
}

std::vector<nix::Symbol> Schema::attrs(nix::EvalState& state) {
    auto attrs = attrSubschemas(state, *this);
    std::vector<nix::Symbol> result;
    for (nix::Attr x : *attrs->attrs) {
        std::string_view s = state.symbols[x.name];
        if (s.starts_with("_"))
            continue;
        result.push_back(x.name);
    }
    return result;
}

Schema Schema::attrSubschema(nix::EvalState& state, nix::Symbol symbol) {
    auto schemas = attrSubschemas(state, *this);
    auto attrsOfAttr =
        schemas->attrs->get(state.symbols.create("_nixAnalyzerAttrsOf"));
    if (attrsOfAttr) {
        return {attrsOfAttr->value};
    }
    auto attr = schemas->attrs->get(symbol);
    if (!attr) {
        auto emptySchema = state.allocValue();
        emptySchema->mkAttrs(state.allocBindings(0));
        return {emptySchema};
    }
    return {attr->value};
}

Schema Schema::functionSubschema(nix::EvalState& state) {
    try {
        state.forceValue(*value, nix::noPos);
        auto vFunction = loadFile(state, "schema/getFunctionSubschema.nix");
        auto vArg = makeAttrs(
            state,
            {
                {"pkgs", nixpkgsValue(state)},
                {"schema", value},
            }
        );
        auto vRes = state.allocValue();
        state.callFunction(*vFunction, *vArg, *vRes, nix::noPos);
        state.forceValue(*vRes, nix::noPos);
        return {vRes};
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        auto emptySchema = state.allocValue();
        emptySchema->mkAttrs(state.allocBindings(0));
        return {emptySchema};
    }
}

std::optional<HoverResult> Schema::hover(nix::EvalState& state) {
    try {
        state.forceValue(*value, nix::noPos);
        std::cerr << "hover of schema " << stringify(state, value) << "\n";
        auto vFunction = loadFile(state, "schema/getSchemaDoc.nix");
        auto vArg = makeAttrs(
            state,
            {
                {"pkgs", nixpkgsValue(state)},
                {"schema", value},
            }
        );
        auto vRes = state.allocValue();
        state.callFunction(*vFunction, *vArg, *vRes, nix::noPos);
        state.forceValue(*vRes, nix::noPos);
        if (vRes->type() == nix::nNull) {
            return {};
        }
        state.forceString(*vRes);
        return {{vRes->string.s}};
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
}

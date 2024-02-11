#include "schema/schema.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>

#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <cstddef>
#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
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

static Schema mkSchema(
    nix::EvalState& state,
    const std::vector<nix::Value*>& values
) {
    std::vector<nix::Value*> result;

    const std::vector<nix::Symbol> nesters{
        state.symbols.create("allOf"),
        state.symbols.create("oneOf"),
        state.symbols.create("anyOf"),
    };

    for (auto value : values) {
        result.push_back(value);
        for (auto nester : nesters) {
            if (auto nested = getAttr(state, value, nester)) {
                try {
                    state.forceList(**nested, nix::noPos);
                } catch (nix::Error &e) {
                    REPORT_ERROR(e);
                }
                for (auto item : nested.value()->listItems()) {
                    result.push_back(item);
                }
            }
        }
    }

    for (auto x : result) {
        state.forceValueDeep(*x);
        std::cerr << stringify(state, x) << "\n";
    }

    return Schema{result};
}

static size_t fileRootIndex(const Analysis& analysis) {
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

static SchemaRoot getSchemaRoot(
    nix::EvalState& state,
    const Analysis& analysis
) {
    if (auto ftype = analysis.fileInfo.ftype) {
        state.forceAttrs(**ftype, nix::noPos);
        // std::cerr << "Using schema: " << stringify(state, ftype.value()) <<
        // "\n";
        if (auto schema =
                getAttr(state, ftype.value(), state.symbols.create("schema"))) {
            return {mkSchema(state, {schema.value()}), fileRootIndex(analysis)};
        }
    }
    return {mkSchema(state, {}), fileRootIndex(analysis)};
}

Schema getSchema(
    nix::EvalState& state,
    const Analysis& analysis
) {
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
                return mkSchema(state, {});
            }
            current = current.attrSubschema(state, *subname);
        } else if (auto lambda = dynamic_cast<nix::ExprLambda*>(child)) {
            current = current.functionSubschema(state);
        } else if (auto list = dynamic_cast<nix::ExprList*>(child)) {
            std::optional<int> subindex;
            for (int i = 0; i < list->elems.size(); i++) {
                if (list->elems[i] == child) {
                    subindex = i;
                }
            }
            if (!subindex) {
                std::cerr
                    << "Failed to find the child in the parent list so don't "
                       "know the index\n";
                return mkSchema(state, {});
            }
            return mkSchema(state, {});
            // path.push_back(*subindex);
        }
    }

    return current;
}

std::vector<nix::Symbol> Schema::attrs(nix::EvalState& state) {
    std::vector<nix::Symbol> result;
    for (auto value : values) {
        auto properties =
            getAttr(state, value, state.symbols.create("properties"));
        if (!properties) {
            continue;
        }
        try {
            state.forceAttrs(*properties.value(), nix::noPos);
        } catch (nix::Error& e) {
            REPORT_ERROR(e);
            continue;
        }
        for (auto attr : *properties.value()->attrs) {
            std::string_view s = state.symbols[attr.name];
            if (s.starts_with("_"))
                continue;
            result.push_back(attr.name);
        }
    }
    return result;
}

Schema Schema::attrSubschema(nix::EvalState& state, nix::Symbol symbol) {
    std::vector<nix::Value*> result;
    for (auto value : values) {
        if (auto properties =
                getAttr(state, value, state.symbols.create("properties"))) {
            if (auto subschema = getAttr(state, properties.value(), symbol)) {
                result.push_back(subschema.value());
            }
        }
        else if (auto additionalProperties = getAttr(
                state, value, state.symbols.create("additionalProperties")
            )) {
            result.push_back(additionalProperties.value());
        }
    }
    return mkSchema(state, result);
}

Schema Schema::functionSubschema(nix::EvalState& state) {
    std::vector<nix::Value*> result;
    for (auto value : values) {
        auto functionTo =
            getAttr(state, value, state.symbols.create("functionTo"));
        if (functionTo) {
            result.push_back({functionTo.value()});
        }
    }
    return mkSchema(state, result);
}

std::optional<HoverResult> Schema::hover(nix::EvalState& state) {
    // try {
    //     state.forceValue(*value, nix::noPos);
    //     std::cerr << "hover of schema " << stringify(state, value) << "\n";
    //     auto vFunction = loadFile(state, "schema/getSchemaDoc.nix");
    //     auto vArg = makeAttrs(
    //         state,
    //         {
    //             {"pkgs", nixpkgsValue(state)},
    //             {"schema", value},
    //         }
    //     );
    //     auto vRes = state.allocValue();
    //     state.callFunction(*vFunction, *vArg, *vRes, nix::noPos);
    //     state.forceValue(*vRes, nix::noPos);
    //     if (vRes->type() == nix::nNull) {
    //         return {};
    //     }
    //     state.forceString(*vRes);
    //     return {{vRes->string.s}};
    // } catch (nix::Error& e) {
    //     REPORT_ERROR(e);
    //     return {};
    // }
    return {};
}

#include "schema/schema.h"
#include "common/analysis.h"

Schema::Schema() {}

Schema::Schema(nix::Value* v) : value(v) {}

nix::Value* functionDescriptionValue(
    nix::EvalState& state,
    nix::Expr* fun,
    nix::Env& env
) {
    auto bindings = state.buildBindings(2);

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
    auto functionV = fun->maybeThunk(state, env);
    bindings.insert(sFunction, functionV);

    auto v = state.allocValue();
    v->mkAttrs(bindings.finish());

    return v;
}

Schema getSchema(nix::EvalState& state, const Analysis& analysis) {
    if (analysis.exprPath.empty())
        return {};

    const std::string getFunctionSchemaPath =
        "/home/josh/dev/nix-analyzer/src/schema/getFunctionSchema.nix";
    auto vGetFunctionSchema = state.allocValue();
    state.evalFile(getFunctionSchemaPath, *vGetFunctionSchema);

    auto vSchema = state.allocValue();
    vSchema->mkAttrs(state.allocBindings(0));
    for (int i = 1; i < analysis.exprPath.size(); i++) {
        nix::ExprCall* call;
        if ((call = dynamic_cast<nix::ExprCall*>(analysis.exprPath[i].e)) &&
            analysis.exprPath[i - 1].e != call->fun) {
            auto vFunctionDescription = functionDescriptionValue(
                state, call->fun, *analysis.exprPath[i].env
            );
            vFunctionDescription->print(state.symbols, std::cout);
            std::cout << "\n";
            state.callFunction(
                *vGetFunctionSchema, *vFunctionDescription, *vSchema, nix::noPos
            );
            vSchema->print(state.symbols, std::cout);
            std::cout << "\n";
        }
    }

    return {};
}
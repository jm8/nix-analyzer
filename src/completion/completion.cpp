#include "completion.h"
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/value.hh>
#include <iostream>
#include <optional>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/logging.h"
#include "common/stringify.h"
#include "schema/schema.h"

std::optional<CompletionResult> completionSelect(
    nix::EvalState& state,
    Analysis& analysis
) {
    auto select = dynamic_cast<nix::ExprSelect*>(analysis.exprPath.front().e);
    if (!select)
        return {};
    std::cerr << "completionSelect\n";
    nix::Value v;
    auto prefixPath = select->attrPath;
    auto attrIndex =
        analysis.attr ? analysis.attr->index : select->attrPath.size() - 1;
    prefixPath.erase(prefixPath.begin() + attrIndex, prefixPath.end());
    auto prefix =
        prefixPath.size() > 0
            ? new nix::ExprSelect(nix::noPos, select->e, prefixPath, nullptr)
            : select->e;
    std::cerr << stringify(state, prefix) << "\n";
    CompletionResult result;
    try {
        auto env = analysis.exprPath.front().env;
        std::cerr << env << "\n";
        prefix->eval(state, *analysis.exprPath.front().env, v);
        state.forceAttrs(v, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return result;
    }
    for (auto [symbol, pos, subValue] : *v.attrs) {
        result.items.push_back(state.symbols[symbol]);
    }
    return result;
}

std::optional<CompletionResult> completionVar(
    nix::EvalState& state,
    Analysis& analysis
) {
    std::cerr << "completionVar\n";
    auto e = analysis.exprPath.front().e;
    const nix::StaticEnv* se = &*state.getStaticEnv(*e);
    CompletionResult result;
    while (se) {
        for (auto [symbol, displ] : se->vars) {
            nix::SymbolStr sym = state.symbols[symbol];
            if (!se->up && std::string_view(sym).rfind("__", 0) == 0) {
                // ignore top level symbols starting with double
                // underscore
                continue;
            }
            result.items.push_back({std::string(sym)});
        }
        se = se->up;
    }
    auto env = analysis.exprPath.front().env;
    while (1) {
        if (env->type == nix::Env::HasWithExpr) {
            nix::Value* v = state.allocValue();
            nix::Expr* e = (nix::Expr*)env->values[0];
            try {
                e->eval(state, *env->up, *v);
                if (v->type() != nix::nAttrs) {
                    // value is %1% while a set was expected
                    v->mkAttrs(state.allocBindings(0));
                }
            } catch (nix::Error& e) {
                REPORT_ERROR(e);
                v->mkAttrs(state.allocBindings(0));
            }
            env->values[0] = v;
            env->type = nix::Env::HasWithAttrs;
        }
        if (env->type == nix::Env::HasWithAttrs) {
            for (auto binding : *env->values[0]->attrs) {
                result.items.push_back({std::string{
                    state.symbols[binding.name]}});
            }
        }
        if (!env->prevWith) {
            break;
        }
        for (size_t l = env->prevWith; l; --l, env = env->up)
            ;
    }
    return result;
}

std::optional<CompletionResult> completionAttrsSchema(
    nix::EvalState& state,
    Analysis& analysis
) {
    auto attrs = dynamic_cast<nix::ExprAttrs*>(analysis.exprPath.front().e);
    if (!attrs)
        return {};
    std::cerr << "completionAttrsSchema\n";
    auto schema = getSchema(state, analysis);
    if (analysis.attr) {
        auto& attrPath = *analysis.attr->attrPath;
        for (size_t i = 0; i < analysis.attr->index; i++) {
            if (!attrPath[i].symbol) {
                std::cerr << "completion of attrsSchema with dynamic attr\n";
                return {};
            }
            schema = schema.attrSubschema(state, attrPath[i].symbol);
        }
    }
    CompletionResult result;
    for (auto sym : schema.attrs(state)) {
        result.items.push_back(state.symbols[sym]);
    }
    return result;
}

CompletionResult completion(nix::EvalState& state, Analysis& analysis) {
    std::cerr << "completing " << exprTypeName(analysis.exprPath.front().e)
              << "\n";
    std::optional<CompletionResult> result;
    if (!result.has_value())
        result = completionSelect(state, analysis);
    if (!result.has_value())
        result = completionAttrsSchema(state, analysis);
    // default to variable completion
    if (!result.has_value())
        result = completionVar(state, analysis);
    return result.value_or(CompletionResult{});
}
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
            std::cerr << "Pushing a thing\n";
            result.items.push_back({std::string(sym)});
        }
        se = se->up;
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
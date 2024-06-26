#include "getlambdaarg.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include <cstddef>
#include <iostream>
#include <vector>
#include "common/analysis.h"
#include "common/evalutil.h"
#include "common/stringify.h"
#include "flakes/evaluateFlake.h"
#include "evaluation/evaluation.h"

std::optional<size_t> topLambdaIndex(const Analysis& analysis) {
    for (int i = analysis.exprPath.size() - 1; i >= 0; i--) {
        if (dynamic_cast<nix::ExprLambda*>(analysis.exprPath[i].e)) {
            return {i};
        }
    }
    return {};
}

nix::Value* getFileLambdaArg(nix::EvalState& state, const Analysis& analysis) {
    auto sArg = state.symbols.create("arg");
    if (analysis.fileInfo.ftype && analysis.fileInfo.ftype->arg) {
        return *analysis.fileInfo.ftype->arg;
    } else {
        auto v = state.allocValue();
        v->mkNull();
        return v;
    }
}

std::optional<size_t> flakeLambdaIndex(
    nix::EvalState& state,
    const Analysis& analysis
) {
    if (!analysis.path.ends_with("/flake.nix"))
        return {};
    if (analysis.exprPath.size() < 2)
        return {};
    auto attrs = dynamic_cast<nix::ExprAttrs*>(analysis.exprPath.back().e);
    if (!attrs) {
        return {};
    }
    auto sOutputs = state.symbols.create("outputs");
    for (auto [symbol, attr] : attrs->attrs) {
        if (symbol == sOutputs &&
            attr.e == analysis.exprPath[analysis.exprPath.size() - 2].e) {
            return {analysis.exprPath.size() - 2};
        }
    }
    return {};
}

void getLambdaArgs(nix::EvalState& state, Analysis& analysis) {
    if (auto i = flakeLambdaIndex(state, analysis)) {
        if (analysis.fileInfo.flakeInputs) {
            analysis.exprPath[*i].lambdaArg = *analysis.fileInfo.flakeInputs;
        } else {
            std::cerr << "flakeInputs is null\n";
        }
    } else if (auto i = topLambdaIndex(analysis)) {
        analysis.exprPath[*i].lambdaArg = getFileLambdaArg(state, analysis);
    }
    for (int i = 0; i < analysis.exprPath.size() - 1; i++) {
        auto parent = analysis.exprPath[i];
        auto child = analysis.exprPath[i+1];

        auto call = dynamic_cast<nix::ExprCall*>(parent.e);
        auto lambda = dynamic_cast<nix::ExprLambda*>(child.e);
        if (call && lambda) {
            std::vector<Diagnostic> diagnostics;
            evaluateWithDiagnostics(state, call, *parent.env, diagnostics);
        }
    }
}

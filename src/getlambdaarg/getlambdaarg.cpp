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

std::optional<size_t> topLambdaIndex(const Analysis& analysis) {
    for (int i = analysis.exprPath.size() - 1; i >= 0; i--) {
        if (dynamic_cast<nix::ExprLambda*>(analysis.exprPath[i].e)) {
            return {i};
        }
    }
    return {};
}

nix::Value* getFileLambdaArg(nix::EvalState& state, const Analysis& analysis) {
    return nixpkgsValue(state);
}

std::optional<size_t> flakeLambdaIndex(
    nix::EvalState& state,
    const Analysis& analysis
) {
    std::cerr << "A\n";
    if (!analysis.path.ends_with("/flake.nix"))
        return {};
    std::cerr << "B\n";
    if (analysis.exprPath.size() < 2)
        return {};
    std::cerr << "C\n";
    auto attrs = dynamic_cast<nix::ExprAttrs*>(analysis.exprPath.back().e);
    if (!attrs) {
        return {};
    }
    std::cerr << "D\n";
    auto sOutputs = state.symbols.create("outputs");
    std::cerr << "E\n";
    for (auto [symbol, attr] : attrs->attrs) {
        if (symbol == sOutputs &&
            attr.e == analysis.exprPath[analysis.exprPath.size() - 2].e) {
            return {analysis.exprPath.size() - 2};
        }
    }
    std::cerr << "F\n";
    return {};
}

void getLambdaArgs(nix::EvalState& state, Analysis& analysis) {
    std::cerr << "GETLAMBDAARGS\n";
    if (auto i = flakeLambdaIndex(state, analysis)) {
        for (auto x : analysis.exprPath) {
            std::cerr << exprTypeName(x.e) << " ";
        }
        std::cerr << "\n";
        std::cerr << "FLAKE LAMBDA ARG AT I=" << *i << "\n";
        if (analysis.fileInfo && analysis.fileInfo->flakeInputs) {
            analysis.exprPath[*i].lambdaArg = *analysis.fileInfo->flakeInputs;
        } else {
            std::cerr << "flakeInputs is null\n";
        }
    } else if (auto i = topLambdaIndex(analysis)) {
        std::cerr << "FILE LAMBDA ARG AT I=" << *i << "\n";

        analysis.exprPath[*i].lambdaArg = getFileLambdaArg(state, analysis);
    } else {
        std::cerr << "NO FILE LAMBDA\n";
    }
}
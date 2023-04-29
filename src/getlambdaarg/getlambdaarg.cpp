#include "getlambdaarg.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include <cstddef>
#include <iostream>
#include "common/analysis.h"
#include "common/evalutil.h"
#include "common/stringify.h"

std::optional<size_t> topLambdaIndex(const Analysis& analysis) {
    for (int i = analysis.exprPath.size() - 1; i >= 0; i--) {
        std::cerr << i << ": " << exprTypeName(analysis.exprPath[i].e) << "\n";
        if (dynamic_cast<nix::ExprLambda*>(analysis.exprPath[i].e)) {
            return {i};
        }
    }
    return {};
}

nix::Value* getFileLambdaArg(nix::EvalState& state, const Analysis& analysis) {
    return makeAttrs(state, {{"pkgs", nixpkgsValue(state)}});
}

void getLambdaArgs(nix::EvalState& state, Analysis& analysis) {
    std::cerr << "GETLAMBDAARGS\n";
    auto i = topLambdaIndex(analysis);
    if (i) {
        std::cerr << "FILE LAMBDA ARG AT I=" << *i << "\n";

        analysis.exprPath[*i].lambdaArg = getFileLambdaArg(state, analysis);
    } else {
        std::cerr << "NO FILE LAMBDA\n";
    }
}
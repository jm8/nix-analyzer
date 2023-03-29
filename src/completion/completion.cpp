#include "completion.h"
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/value.hh>
#include <iostream>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/stringify.h"

CompletionResult completionSelect(nix::EvalState& state, Analysis& analysis) {
    auto select = dynamic_cast<nix::ExprSelect*>(analysis.exprPath.front().e);
    if (!select)
        return {};
    std::cerr << "completionSelect\n";
    nix::Value v;
    try {
        state.eval(select->e, v);
        state.forceAttrs(v, nix::noPos);
    } catch (nix::Error& e) {
        std::cerr << "caught error: " << e.info().msg << "\n";
        return {};
    }
    CompletionResult result;
    for (auto [symbol, pos, subValue] : *v.attrs) {
        result.items.push_back(state.symbols[symbol]);
    }
    return result;
}

CompletionResult completion(nix::EvalState& state, Analysis& analysis) {
    std::cerr << "completing " << exprTypeName(analysis.exprPath.front().e)
              << "\n";
    CompletionResult result;
    if (result.items.empty())
        result = completionSelect(state, analysis);
    return result;
}
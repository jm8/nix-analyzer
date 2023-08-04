#include "evaluation.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/value.hh>
#include <vector>
#include "common/analysis.h"
#include "common/logging.h"

nix::Value* evaluateWithDiagnostics(
    nix::EvalState& state,
    nix::Expr* e,
    nix::Env& env,
    std::vector<Diagnostic>& diagnostics
) {
    auto v = state.allocValue();
    v->mkNull();
    try {
        e->eval(state, env, *v);
    } catch (nix::Error& err) {
        REPORT_ERROR(err);
        diagnostics.push_back(
            {nix::filterANSIEscapes(err.info().msg.str(), true),
             {{0, 0}, {999, 999}}}
        );
    }
    return v;
}
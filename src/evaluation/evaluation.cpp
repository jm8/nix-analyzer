#include "evaluation.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/value.hh>
#include <iostream>
#include <vector>
#include "common/analysis.h"
#include "common/logging.h"
#include "nixexpr.hh"
#include "util.hh"

nix::Value* evaluateWithDiagnostics(
    nix::EvalState& state,
    nix::Expr* e,
    nix::Env& env,
    std::vector<Diagnostic>& diagnostics
) {
    auto push_error = [&](nix::Expr* e, nix::Error& error) {
        diagnostics.push_back(
            {nix::filterANSIEscapes(error.info().msg.str(), true),
             Location(state, e).range}
        );
    };

    nix::Value* v;
    try {
        v = e->maybeThunk(state, env);
    } catch (nix::Error& error) {
        push_error(e, error);
        v = state.allocValue();
        v->mkNull();
        return v;
    }

    std::set<const nix::Value*> seen;

    std::function<void(nix::Value & v, int)> recurse;

    recurse = [&](nix::Value& v, int depth) {
        if (!seen.insert(&v).second)
            return;
        if (depth <= 0)
            return;

        if (v.isThunk()) {
            nix::Env* env = v.thunk.env;
            nix::Expr* expr = v.thunk.expr;
            if (auto let = dynamic_cast<nix::ExprLet*>(expr)) {
                nix::Displacement displ = 0;
                nix::Env& env2(state.allocEnv(let->attrs->attrs.size()));
                env2.up = env;
                for (auto& i : let->attrs->attrs) {
                    auto subvalue = i.second.e->maybeThunk(
                        state, i.second.inherited ? *env : env2
                    );
                    recurse(*subvalue, depth-1);
                    env2.values[displ++] = subvalue;
                }
                let->body->eval(state, env2, v);
            } else {
                try {
                    v.mkBlackhole();
                    expr->eval(state, *env, v);
                } catch (nix::Error& error) {
                    v.mkThunk(env, expr);
                    push_error(expr, error);
                }
            }
        } else if (v.isApp()) {
            try {
                state.callFunction(*v.app.left, *v.app.right, v, nix::noPos);
            } catch (nix::Error& error) {
                std::cerr << error.msg() << "\n";
                push_error(e, error);
            }
        } else if (v.isBlackhole()) {
            return;
        }

        if (v.type() == nix::nAttrs) {
            for (auto& i : *v.attrs) {
                recurse(*i.value, depth - 1);
            }
        }

        else if (v.isList()) {
            for (auto v2 : v.listItems()) {
                recurse(*v2, depth - 1);
            }
        }
    };

    recurse(*v, 5);

    return v;
}

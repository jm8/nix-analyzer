#include "na_config.h"
#include <nix/eval.hh>
#include <iostream>
#include "common/logging.h"
#include "common/stringify.h"
#include "document/document.h"

nix::Env* Document::updateEnv(
    nix::Expr* parent,
    nix::Expr* child,
    nix::Env* up,
    std::optional<nix::Value*> lambdaArg
) {
    if (auto let = dynamic_cast<nix::ExprLet*>(parent)) {
        nix::Env* env2 = &state.allocEnv(let->attrs->attrs.size());
        env2->up = up;

        nix::Displacement displ = 0;

        // if sub is an inherited let binding use the env from the parent.
        // if it is a non-inherited let binding or the body use env2.
        bool useSuperEnv = false;

        for (auto& [symbol, attrDef] : let->attrs->attrs) {
            env2->values[displ] =
                thunk(attrDef.e, attrDef.inherited ? up : env2);
            displ++;
            if (attrDef.e == child && attrDef.inherited)
                useSuperEnv = true;
        }
        return useSuperEnv ? up : env2;
    }
    if (auto lambda = dynamic_cast<nix::ExprLambda*>(parent)) {
        auto size =
            (!lambda->arg ? 0 : 1) +
            (lambda->hasFormals() ? lambda->formals->formals.size() : 0);
        nix::Env* env2 = &state.allocEnv(size);
        env2->up = up;

        nix::Value* arg;
        if (lambdaArg) {
            arg = *lambdaArg;
        } else {
            arg = state.allocValue();
            arg->mkNull();
        }

        nix::Displacement displ = 0;

        if (!lambda->hasFormals()) {
            env2->values[displ++] = arg;
        } else {
            try {
                state.forceAttrs(*arg, nix::noPos, "");
            } catch (nix::Error& e) {
                REPORT_ERROR(e);
                for (uint32_t i = 0; i < lambda->formals->formals.size(); i++) {
                    nix::Value* val = state.allocValue();
                    val->mkNull();
                    env2->values[displ++] = val;
                }
                return env2;
            }

            if (lambda->arg) {
                env2->values[displ++] = arg;
            }

            /* For each formal argument, get the actual argument.  If
               there is no matching actual argument but the formal
               argument has a default, use the default. */
            for (auto& i : lambda->formals->formals) {
                auto j = arg->attrs->get(i.name);
                if (!j) {
                    nix::Value* val;
                    if (i.def) {
                        val = thunk(i.def, env2);
                    } else {
                        val = state.allocValue();
                        val->mkNull();
                    }
                    env2->values[displ++] = val;
                } else {
                    env2->values[displ++] = j->value;
                }
            }
        }
        return env2;
    }
    if (auto exprAttrs = dynamic_cast<nix::ExprAttrs*>(parent)) {
        if (!exprAttrs->recursive) {
            return up;
        }

        nix::Env* env2 = &state.allocEnv(exprAttrs->attrs.size());
        env2->up = up;

        // ignoring __overrides

        /* The recursive attributes are evaluated in the new
           environment, while the inherited attributes are evaluated
           in the original environment. */

        nix::Displacement displ = 0;
        for (auto& i : exprAttrs->attrs) {
            nix::Value* vAttr;
            vAttr = thunk(i.second.e, i.second.inherited ? up : env2);
            env2->values[displ] = vAttr;
            displ++;
        }
        return env2;
    }
    if (auto with = dynamic_cast<nix::ExprWith*>(parent)) {
        if (child != with->body) {
            return up;
        }
        nix::Env* env2 = &state.allocEnv(1);
        env2->up = up;
        env2->values[0] = (nix::Value*)with->attrs;
        return env2;
    }
    return up;
}

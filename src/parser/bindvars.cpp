#include "na_config.h"
#include "bindvars.h"
#include <unordered_map>
#include "nixexpr.hh"

// Copy+paste of Expr::bindVars modified to not throw exception

void bindVars(
    nix::EvalState& state,
    Document& document,
    std::shared_ptr<nix::StaticEnv> env,
    nix::Expr* expr
) {
    document.exprData[expr].staticEnv = env;

    if (auto e = dynamic_cast<nix::ExprVar*>(expr)) {
        /* Check whether the variable appears in the environment.  If so,
           set its level and displacement. */
        const nix::StaticEnv* curEnv;
        nix::Level level;
        int withLevel = -1;
        for (curEnv = env.get(), level = 0; curEnv;
             curEnv = curEnv->up, level++) {
            if (curEnv->isWith) {
                if (withLevel == -1)
                    withLevel = level;
            } else {
                auto i = curEnv->find(e->name);
                if (i != curEnv->vars.end()) {
                    e->fromWith = nullptr;
                    e->level = level;
                    e->displ = i->second;
                    return;
                }
            }
        }

        /* Otherwise, the variable must be obtained from the nearest
           enclosing `with'.  If there is no `with', then we can issue an
           "undefined variable" error now. */
        if (withLevel == -1) {
            document.parseErrors.push_back(
                {hintfmt("undefined variable '%1%'", state.symbols[e->name])
                     .str(),
                 document.tokenRangeToRange(document.exprData[e].range)}
            );
        }
        for (nix::StaticEnv const* currEnv = env.get(); currEnv && !e->fromWith;
             currEnv = currEnv->up)
            e->fromWith = currEnv->isWith;
        e->level = withLevel;
    }

    if (auto e = dynamic_cast<nix::ExprSelect*>(expr)) {
        bindVars(state, document, env, e->e);
        if (e->def)
            bindVars(state, document, env, e->def);
        for (auto& i : e->attrPath)
            if (!i.symbol)
                bindVars(state, document, env, i.expr);
    }

    if (auto e = dynamic_cast<nix::ExprOpHasAttr*>(expr)) {
        bindVars(state, document, env, e->e);
        for (auto& i : e->attrPath)
            if (!i.symbol)
                bindVars(state, document, env, i.expr);
    }

    if (auto e = dynamic_cast<nix::ExprAttrs*>(expr)) {
        if (e->recursive) {
            auto newEnv = std::make_shared<nix::StaticEnv>(
                nullptr, env.get(), e->attrs.size()
            );

            nix::Displacement displ = 0;
            for (auto& i : e->attrs)
                newEnv->vars.emplace_back(i.first, i.second.displ = displ++);

            // No need to sort newEnv since attrs is in sorted order.

            for (auto& i : e->attrs)
                bindVars(
                    state,
                    document,
                    i.second.inherited ? env : newEnv,
                    i.second.e
                );

            for (auto& i : e->dynamicAttrs) {
                bindVars(state, document, newEnv, i.nameExpr);
                bindVars(state, document, newEnv, i.valueExpr);
            }
        } else {
            for (auto& i : e->attrs)
                bindVars(state, document, env, i.second.e);

            for (auto& i : e->dynamicAttrs) {
                bindVars(state, document, env, i.nameExpr);
                bindVars(state, document, env, i.valueExpr);
            }
        }
    }

    if (auto e = dynamic_cast<nix::ExprList*>(expr)) {
        for (auto& i : e->elems)
            bindVars(state, document, env, i);
    }

    if (auto e = dynamic_cast<nix::ExprLambda*>(expr)) {
        auto newEnv = std::make_shared<nix::StaticEnv>(
            nullptr,
            env.get(),
            (e->hasFormals() ? e->formals->formals.size() : 0) +
                (!e->arg ? 0 : 1)
        );

        nix::Displacement displ = 0;

        if (e->arg)
            newEnv->vars.emplace_back(e->arg, displ++);

        if (e->hasFormals()) {
            for (auto& i : e->formals->formals)
                newEnv->vars.emplace_back(i.name, displ++);

            newEnv->sort();

            for (auto& i : e->formals->formals)
                if (i.def)
                    bindVars(state, document, newEnv, i.def);
        }

        bindVars(state, document, newEnv, e->body);
    }

    if (auto e = dynamic_cast<nix::ExprCall*>(expr)) {
        bindVars(state, document, env, e->fun);
        for (auto item : e->args)
            bindVars(state, document, env, item);
    }

    if (auto e = dynamic_cast<nix::ExprLet*>(expr)) {
        auto newEnv = std::make_shared<nix::StaticEnv>(
            nullptr, env.get(), e->attrs->attrs.size()
        );

        nix::Displacement displ = 0;
        for (auto& i : e->attrs->attrs)
            newEnv->vars.emplace_back(i.first, i.second.displ = displ++);

        // No need to sort newEnv since attrs->attrs is in sorted order.

        for (auto& i : e->attrs->attrs)
            bindVars(
                state, document, i.second.inherited ? env : newEnv, i.second.e
            );

        bindVars(state, document, newEnv, e->body);
    }

    if (auto e = dynamic_cast<nix::ExprWith*>(expr)) {
        /* Does this `with' have an enclosing `with'?  If so, record its
           level so that `lookupVar' can look up variables in the previous
           `with' if this one doesn't contain the desired attribute. */
        const nix::StaticEnv* curEnv;
        nix::Level level;
        e->prevWith = 0;
        for (curEnv = env.get(), level = 1; curEnv;
             curEnv = curEnv->up, level++)
            if (curEnv->isWith) {
                e->prevWith = level;
                break;
            }

        bindVars(state, document, env, e->attrs);
        auto newEnv = std::make_shared<nix::StaticEnv>(e, env.get());
        bindVars(state, document, newEnv, e->body);
    }

    if (auto e = dynamic_cast<nix::ExprIf*>(expr)) {
        bindVars(state, document, env, e->cond);
        bindVars(state, document, env, e->then);
        bindVars(state, document, env, e->else_);
    }

    if (auto e = dynamic_cast<nix::ExprAssert*>(expr)) {
        bindVars(state, document, env, e->cond);
        bindVars(state, document, env, e->body);
    }

    if (auto e = dynamic_cast<nix::ExprOpNot*>(expr)) {
        bindVars(state, document, env, e->e);
    }

    if (auto e = dynamic_cast<nix::ExprConcatStrings*>(expr)) {
        for (auto& i : *e->es)
            bindVars(state, document, env, i.second);
    }

    if (auto e = dynamic_cast<nix::ExprPos*>(expr)) {
        // do nothing
    }
}
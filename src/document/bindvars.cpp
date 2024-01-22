#include "na_config.h"
#include <nix/nixexpr.hh>
#include <unordered_map>
#include "document/document.h"

// Copy+paste of Expr::bindVars modified to not throw exception
// Sets exprData staticEnv and parent

void Document::bindVars(
    std::shared_ptr<nix::StaticEnv> env,
    nix::Expr* expr,
    std::optional<nix::Expr*> parent
) {
    exprData[expr].staticEnv = env;
    exprData[expr].parent = parent;

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
            parseErrors.push_back(
                {hintfmt("undefined variable '%1%'", state.symbols[e->name])
                     .str(),
                 tokenRangeToRange(exprData[e].range)}
            );
        }
        for (nix::StaticEnv const* currEnv = env.get(); currEnv && !e->fromWith;
             currEnv = currEnv->up)
            e->fromWith = currEnv->isWith;
        e->level = withLevel;
    }

    if (auto e = dynamic_cast<nix::ExprSelect*>(expr)) {
        bindVars(env, e->e, expr);
        if (e->def)
            bindVars(env, e->def, expr);
        for (auto& i : e->attrPath)
            if (!i.symbol)
                bindVars(env, i.expr, expr);
    }

    if (auto e = dynamic_cast<nix::ExprOpHasAttr*>(expr)) {
        bindVars(env, e->e, expr);
        for (auto& i : e->attrPath)
            if (!i.symbol)
                bindVars(env, i.expr, expr);
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
                bindVars(i.second.inherited ? env : newEnv, i.second.e, expr);

            for (auto& i : e->dynamicAttrs) {
                bindVars(newEnv, i.nameExpr, expr);
                bindVars(newEnv, i.valueExpr, expr);
            }
        } else {
            for (auto& i : e->attrs)
                bindVars(env, i.second.e, expr);

            for (auto& i : e->dynamicAttrs) {
                bindVars(env, i.nameExpr, expr);
                bindVars(env, i.valueExpr, expr);
            }
        }
    }

    if (auto e = dynamic_cast<nix::ExprList*>(expr)) {
        for (auto& i : e->elems)
            bindVars(env, i, expr);
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
                    bindVars(newEnv, i.def, expr);
        }

        bindVars(newEnv, e->body, expr);
    }

    if (auto e = dynamic_cast<nix::ExprCall*>(expr)) {
        bindVars(env, e->fun, expr);
        for (auto item : e->args)
            bindVars(env, item, expr);
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
            bindVars(i.second.inherited ? env : newEnv, i.second.e, expr);

        bindVars(newEnv, e->body, expr);
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

        bindVars(env, e->attrs, expr);
        auto newEnv = std::make_shared<nix::StaticEnv>(e, env.get());
        bindVars(newEnv, e->body, expr);
    }

    if (auto e = dynamic_cast<nix::ExprIf*>(expr)) {
        bindVars(env, e->cond, expr);
        bindVars(env, e->then, expr);
        bindVars(env, e->else_, expr);
    }

    if (auto e = dynamic_cast<nix::ExprAssert*>(expr)) {
        bindVars(env, e->cond, expr);
        bindVars(env, e->body, expr);
    }

    if (auto e = dynamic_cast<nix::ExprOpNot*>(expr)) {
        bindVars(env, e->e, expr);
    }

    if (auto e = dynamic_cast<nix::ExprConcatStrings*>(expr)) {
        for (auto& i : *e->es)
            bindVars(env, i.second, expr);
    }

    if (auto e = dynamic_cast<nix::ExprPos*>(expr)) {
        // do nothing
    }
}
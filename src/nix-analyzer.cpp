#include "nix-analyzer.h"
#include <iostream>
#include <sstream>
#include "debug.h"

#include "error.hh"
#include "flake/flake.hh"
#include "globals.hh"
#include "nixexpr.hh"
#include "url.hh"

using namespace std;
using namespace nix;
using namespace nix::flake;

NixAnalyzer::NixAnalyzer(const Strings& searchPath,
                         nix::ref<Store> store,
                         ::Logger& log)
    : log(log) {
    nix::settings.experimentalFeatures.set("flakes", true);
    state = make_unique<EvalState>(searchPath, store);
}

int poscmp(Pos a, Pos b) {
    if (a.line > b.line) {
        return 1;
    }
    if (a.line < b.line) {
        return -1;
    }
    if (a.column > b.column) {
        return 1;
    }
    if (a.column < b.column) {
        return -1;
    }
    return 0;
}

Analysis NixAnalyzer::getExprPath(string source,
                                  Path path,
                                  Path basePath,
                                  Pos targetPos) {
    vector<Expr*> exprPath;
    vector<Spanned<ExprPath*>> paths;
    vector<ParseError> errors;
    state->parseWithCallback(
        source, path.empty() ? nix::foString : nix::foFile, path, basePath,
        state->staticBaseEnv,
        [&](Expr* e, Pos start, Pos end) {
            if (start.origin != targetPos.origin ||
                start.file != targetPos.file) {
                return;
            }

            if (auto path = dynamic_cast<ExprPath*>(e)) {
                paths.push_back({path, start, end});
            }

            if (!(poscmp(start, targetPos) <= 0 &&
                  poscmp(targetPos, end) <= 0)) {
                return;
            }

            exprPath.push_back(e);
        },
        [&errors](ParseError error) { errors.push_back(error); });
    return {exprPath, errors, path, basePath, paths};
}

vector<NACompletionItem> NixAnalyzer::complete(vector<Expr*> exprPath,
                                               FileInfo file) {
    if (exprPath.empty()) {
        log.info("Completing empty exprPath");
        vector<NACompletionItem> result;
        for (auto [symbol, displ] : state->staticBaseEnv->vars) {
            SymbolStr sym = state->symbols[symbol];
            if (string_view(sym).rfind("__", 0) == 0) {
                // ignore top level symbols starting with double
                // underscore
                continue;
            }
            result.push_back(NACompletionItem{
                string(sym), NACompletionItem::Type::Variable});
        }
        return result;
    }

    log.info("Completing ", exprTypeName(exprPath.front()));

    vector<optional<Value*>> lambdaArgs = calculateLambdaArgs(exprPath, file);
    Env* env = calculateEnv(exprPath, lambdaArgs, file);

    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        log.info("Completing ExprSelect FOR REAL");
        AttrPath path(select->attrPath.begin(), select->attrPath.end() - 1);
        ExprSelect prefix(select->pos, select->e, path, select->def);
        Value v;
        try {
            prefix.eval(*state, *env, v);
            state->forceValue(v, select->pos);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            return {};
        }

        if (v.type() != nAttrs) {
            return {};
        }
        vector<NACompletionItem> result;
        for (auto attr : *v.attrs) {
            result.push_back(
                {state->symbols[attr.name], NACompletionItem::Type::Property});
        }
        return result;
    }

    if (auto attrs = dynamic_cast<ExprAttrs*>(exprPath.front())) {
        if (exprPath.size() == 1) {
            return {};
        }
        if (auto schema = getSchema(exprPath[1], attrs)) {
            vector<NACompletionItem> result;
            for (auto item : schema->items) {
                result.push_back(
                    {item.name, NACompletionItem::Type::Field, item.doc});
            }
            return result;
        }
    }

    log.info("Defaulting to variable completion");
    vector<NACompletionItem> result;
    const StaticEnv* se = state->getStaticEnv(*exprPath.front()).get();
    while (se) {
        for (auto [symbol, displ] : se->vars) {
            SymbolStr sym = state->symbols[symbol];
            if (!se->up && string_view(sym).rfind("__", 0) == 0) {
                // ignore top level symbols starting with double
                // underscore
                continue;
            }
            result.push_back({string(sym), NACompletionItem::Type::Variable});
        }
        se = se->up;
    }
    while (1) {
        if (env->type == Env::HasWithExpr) {
            Value* v = state->allocValue();
            Expr* e = (Expr*)env->values[0];
            stringstream ss;
            e->show(state->symbols, ss);
            try {
                e->eval(*state, *env->up, *v);
                if (v->type() != nAttrs) {
                    // value is %1% while a set was expected
                    v->mkAttrs(state->allocBindings(0));
                }
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                v->mkAttrs(state->allocBindings(0));
            }
            env->values[0] = v;
            env->type = Env::HasWithAttrs;
        }
        if (env->type == Env::HasWithAttrs) {
            for (auto binding : *env->values[0]->attrs) {
                log.info("Binding ", state->symbols[binding.name]);
                result.push_back({state->symbols[binding.name],
                                  NACompletionItem::Type::Variable});
            }
        }
        if (!env->prevWith) {
            break;
        }
        for (size_t l = env->prevWith; l; --l, env = env->up)
            ;
    }
    return result;
}

Env* NixAnalyzer::calculateEnv(vector<Expr*> exprPath,
                               vector<optional<Value*>> lambdaArgs,
                               FileInfo file) {
    Env* env = &state->baseEnv;
    for (size_t i = exprPath.size() - 1; i >= 1; i--) {
        Expr* child = exprPath[i - 1];
        Expr* parent = exprPath[i];
        env = updateEnv(parent, child, env, lambdaArgs[i]);
    }
    return env;
}

Env* NixAnalyzer::updateEnv(Expr* parent,
                            Expr* child,
                            Env* up,
                            optional<Value*> lambdaArg) {
    if (auto let = dynamic_cast<ExprLet*>(parent)) {
        Env* env2 = &state->allocEnv(let->attrs->attrs.size());
        env2->up = up;

        Displacement displ = 0;

        // if sub is an inherited let binding use the env from the parent.
        // if it is a non-inherited let binding or the body use env2.
        bool useSuperEnv = false;

        for (auto& [symbol, attrDef] : let->attrs->attrs) {
            env2->values[displ++] =
                attrDef.e->maybeThunk(*state, attrDef.inherited ? *up : *env2);
            if (attrDef.e == child && attrDef.inherited)
                useSuperEnv = true;
        }
        return useSuperEnv ? up : env2;
    }
    if (auto lambda = dynamic_cast<ExprLambda*>(parent)) {
        auto size =
            (!lambda->arg ? 0 : 1) +
            (lambda->hasFormals() ? lambda->formals->formals.size() : 0);
        Env* env2 = &state->allocEnv(size);
        env2->up = up;

        Value* arg;
        if (lambdaArg) {
            arg = *lambdaArg;
        } else {
            arg = state->allocValue();
            arg->mkNull();
        }

        Displacement displ = 0;

        if (!lambda->hasFormals()) {
            env2->values[displ++] = arg;
        } else {
            try {
                state->forceAttrs(*arg, noPos);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                for (uint32_t i = 0; i < lambda->formals->formals.size(); i++) {
                    Value* val = state->allocValue();
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
                    Value* val;
                    if (i.def) {
                        try {
                            val = i.def->maybeThunk(*state, *env2);
                        } catch (Error& e) {
                            log.info("Caught error: ", e.info().msg.str());
                            val = state->allocValue();
                            val->mkNull();
                        }
                    } else {
                        val = state->allocValue();
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
    if (auto exprAttrs = dynamic_cast<ExprAttrs*>(parent)) {
        if (!exprAttrs->recursive) {
            return up;
        }

        Env* env2 = &state->allocEnv(exprAttrs->attrs.size());
        env2->up = up;

        // ignoring __overrides

        /* The recursive attributes are evaluated in the new
           environment, while the inherited attributes are evaluated
           in the original environment. */

        Displacement displ = 0;
        for (auto& i : exprAttrs->attrs) {
            Value* vAttr;
            try {
                vAttr = i.second.e->maybeThunk(
                    *state, i.second.inherited ? *up : *env2);
            } catch (Error& e) {
                vAttr = state->allocValue();
                vAttr->mkNull();
            }
            env2->values[displ++] = vAttr;
        }
        return env2;
    }
    if (auto with = dynamic_cast<ExprWith*>(parent)) {
        if (child != with->body) {
            return up;
        }
        Env* env2 = &state->allocEnv(1);
        env2->up = up;
        env2->prevWith = with->prevWith;
        env2->type = Env::HasWithExpr;
        env2->values[0] = (Value*)with->attrs;
        return env2;
    }
    return up;
}

vector<optional<Value*>> NixAnalyzer::calculateLambdaArgs(
    vector<Expr*> exprPath,
    FileInfo file) {
    if (exprPath.empty()) {
        return {};
    }
    vector<optional<Value*>> result(exprPath.size());

    bool firstLambda = true;
    ExprLambda* e;

    // how to loop backwards https://stackoverflow.com/a/3611799
    for (size_t i = exprPath.size(); i-- != 0;) {
        e = dynamic_cast<ExprLambda*>(exprPath[i]);
        if (!e) {
            continue;
        }
        if (firstLambda && file.type == FileType::Package) {
            try {
                Value* v = state->allocValue();
                Value fun;
                Value arg;
                arg.mkAttrs(state->allocBindings(0));
                state->evalFile(file.nixpkgs() + "/default.nix"s, fun);
                state->callFunction(fun, arg, *v, noPos);
                result[i] = v;
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
            }
        }
        firstLambda = false;
    }

    if (file.type == FileType::Flake) {
        Value vInfo;
        try {
            exprPath.back()->eval(*state, state->baseEnv, vInfo);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            vInfo.mkNull();
        }

        auto sInputs = state->symbols.create("inputs");
        vInfo.print(state->symbols, cerr);
        cerr << endl;
        if (vInfo.type() == nAttrs) {
            if (auto inputsAttr = vInfo.attrs->get(sInputs)) {
                state->forceValue(*inputsAttr->value, noPos);
                inputsAttr->value->print(state->symbols, cerr);
                cerr << endl;
                auto inputs = parseFlakeInputs(*state, inputsAttr->value,
                                               inputsAttr->pos, file.path, {});
                for (auto& [flakeId, input] : inputs) {
                }
            }
        }
    }

    return result;
}

optional<Schema> NixAnalyzer::getSchema(Expr* parent, Expr* child) {
    if (auto call = dynamic_cast<ExprCall*>(parent)) {
        if (child == call->fun) {
            return {};
        }
        ExprSelect* select = dynamic_cast<ExprSelect*>(call->fun);
        ExprVar* var = dynamic_cast<ExprVar*>(call->fun);
        if ((select && state->symbols[select->attrPath.back().symbol] ==
                           "mkDerivation") ||
            (var && state->symbols[var->name] == "mkDerivation")) {
            log.info("Completing with schemaMkDerivation");
            return schemaMkDerivation;
        }
    }
    return {};
}

Path FileInfo::nixpkgs() {
    return "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source";
}
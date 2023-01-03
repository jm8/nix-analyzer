#include "nix-analyzer.h"
#include <iostream>
#include "debug.h"

#include "error.hh"
#include "nixexpr.hh"
#include "url.hh"

using namespace std;
using namespace nix;

NixAnalyzer::NixAnalyzer(const Strings& searchPath, nix::ref<Store> store)
    : state(make_unique<EvalState>(searchPath, store)) {
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
    vector<ParseError> errors;
    state->parseWithCallback(
        source, path.empty() ? nix::foString : nix::foFile, path, basePath,
        state->staticBaseEnv,
        [targetPos, &exprPath](Expr* e, Pos start, Pos end) {
            if (start.origin != targetPos.origin ||
                start.file != targetPos.file) {
                return;
            }

            if (!(poscmp(start, targetPos) <= 0 &&
                  poscmp(targetPos, end) <= 0)) {
                return;
            }

            exprPath.push_back(e);
        },
        [&errors](ParseError error) { errors.push_back(error); });
    return {exprPath, errors};
}

vector<CompletionItem> NixAnalyzer::complete(vector<Expr*> exprPath,
                                             FileInfo file) {
    if (exprPath.empty()) {
        vector<CompletionItem> result;
        for (auto [symbol, displ] : state->staticBaseEnv->vars) {
            SymbolStr sym = state->symbols[symbol];
            if (string_view(sym).rfind("__", 0) == 0) {
                // ignore top level symbols starting with double
                // underscore
                continue;
            }
            result.push_back(
                CompletionItem{string(sym), CompletionItem::Type::Variable});
        }
        return result;
    }

    vector<optional<Value*>> lambdaArgs = calculateLambdaArgs(exprPath, file);
    Env* env = calculateEnv(exprPath, lambdaArgs, file);

    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        AttrPath path(select->attrPath.begin(), select->attrPath.end() - 1);
        ExprSelect prefix(select->pos, select->e, path, select->def);
        Value v;

        try {
            prefix.eval(*state, *env, v);
        } catch (Error& e) {
            return {};
        }

        if (v.type() != nAttrs) {
            return {};
        }

        vector<CompletionItem> result;
        for (auto attr : *v.attrs) {
            result.push_back(
                {state->symbols[attr.name], CompletionItem::Type::Property});
        }
        return result;
    }

    vector<CompletionItem> result;
    const StaticEnv* se = state->getStaticEnv(*exprPath.front()).get();
    while (se) {
        for (auto [symbol, displ] : se->vars) {
            SymbolStr sym = state->symbols[symbol];
            if (!se->up && string_view(sym).rfind("__", 0) == 0) {
                // ignore top level symbols starting with double
                // underscore
                continue;
            }
            result.push_back({string(sym), CompletionItem::Type::Variable});
        }
        se = se->up;
    }
    return result;
}

Env* NixAnalyzer::calculateEnv(vector<Expr*> exprPath,
                               vector<optional<Value*>> lambdaArgs,
                               FileInfo file) {
    Env* env = &state->baseEnv;
    for (size_t i = exprPath.size() - 1; i >= 1; i--) {
        Expr* sub = exprPath[i - 1];
        Expr* super = exprPath[i];
        env = updateEnv(super, sub, env, lambdaArgs[i]);
    }
    return env;
}

Env* NixAnalyzer::updateEnv(Expr* super,
                            Expr* sub,
                            Env* up,
                            optional<Value*> lambdaArg) {
    if (auto let = dynamic_cast<ExprLet*>(super)) {
        Env* env2 = &state->allocEnv(let->attrs->attrs.size());
        env2->up = up;

        Displacement displ = 0;

        // if sub is an inherited let binding use the env from the parent.
        // if it is a non-inherited let binding or the body use env2.
        bool useSuperEnv = false;

        for (auto& [symbol, attrDef] : let->attrs->attrs) {
            env2->values[displ++] =
                attrDef.e->maybeThunk(*state, attrDef.inherited ? *up : *env2);
            if (attrDef.e == sub && attrDef.inherited)
                useSuperEnv = true;
        }
        return useSuperEnv ? up : env2;
    }
    if (auto lambda = dynamic_cast<ExprLambda*>(super)) {
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
            Value* v = state->allocValue();
            try {
                state->evalFile(file.nixpkgs() + "/default.nix"s, *v);
                result[i] = v;
            } catch (Error& e) {
            }
        }
        firstLambda = false;
    }

    return result;
}

Path FileInfo::nixpkgs() {
    return "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source";
}
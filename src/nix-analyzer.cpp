#include "nix-analyzer.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <variant>
#include "debug.h"

#include "error.hh"
#include "eval.hh"
#include "flake/flake.hh"
#include "flake/lockfile.hh"
#include "globals.hh"
#include "nixexpr.hh"
#include "url.hh"
#include "value.hh"

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

pair<NACompletionType, vector<NACompletionItem>> NixAnalyzer::complete(
    vector<Expr*> exprPath,
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
            result.push_back(NACompletionItem{string(sym)});
        }
        return {NACompletionType::Variable, result};
    }

    log.info("Completing ", exprTypeName(exprPath.front()));

    vector<optional<Value*>> lambdaArgs = calculateLambdaArgs(exprPath, file);
    Env* env = calculateEnv(exprPath, lambdaArgs, file);

    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
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
            result.push_back({state->symbols[attr.name]});
        }
        return {NACompletionType::Property, result};
    }

    if (auto attrs = dynamic_cast<ExprAttrs*>(exprPath.front())) {
        if (exprPath.size() == 1) {
            return {};
        }
        if (auto schema = getSchema(*env, exprPath[1], attrs)) {
            vector<NACompletionItem> result;
            for (auto item : schema->items) {
                result.push_back({item.name, item.doc});
            }
            return {NACompletionType::Field, result};
        }
        return {};
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
            result.push_back({string(sym)});
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
                result.push_back({state->symbols[binding.name]});
            }
        }
        if (!env->prevWith) {
            break;
        }
        for (size_t l = env->prevWith; l; --l, env = env->up)
            ;
    }
    return {NACompletionType::Variable, result};
}

NAHoverResult NixAnalyzer::hover(vector<Expr*> exprPath, FileInfo file) {
    if (exprPath.empty()) {
        log.info("hover of empty exprPath");
        return {};
    };
    log.info("hover of ", exprTypeName(exprPath.front()));
    auto lambdaArgs = calculateLambdaArgs(exprPath, file);
    Env* env = calculateEnv(exprPath, lambdaArgs, file);
    if (auto var = dynamic_cast<ExprVar*>(exprPath.front())) {
        Value v;
        try {
            var->eval(*state, *env, v);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            v.mkNull();
        }
        PosIdx posIdx = v.definitionPos;
        if (posIdx) {
            return {{stringifyValue(*state, v)}, {state->positions[posIdx]}};
        } else {
            log.info("Pos doesn't exist.");
            return {{stringifyValue(*state, v)}, {}};
        }
    }
    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        Value v;
        try {
            // TODO: Evaluate up to the one that is selected.
            select->eval(*state, *env, v);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            v.mkNull();
        }
        PosIdx posIdx = v.definitionPos;
        if (posIdx) {
            return {{stringifyValue(*state, v)}, {state->positions[posIdx]}};
        } else {
            log.info("Pos doesn't exist.");
            return {{stringifyValue(*state, v)}, {}};
        }
    }
    if (auto path = dynamic_cast<ExprPath*>(exprPath.front())) {
        Path resolved = path->s;
        try {
            resolved = nix::resolveExprPath(resolved);
        } catch (Error& e) {
        }
        return {{resolved}, {{resolved, foFile, 1, 1}}};
    }
    return {};
}

Env* NixAnalyzer::calculateEnv(vector<Expr*> exprPath,
                               vector<optional<Value*>> lambdaArgs,
                               FileInfo file) {
    log.info("Entering calculateEnv");
    Env* env = &state->baseEnv;
    for (size_t i = exprPath.size() - 1; i >= 1; i--) {
        Expr* child = exprPath[i - 1];
        Expr* parent = exprPath[i];
        env = updateEnv(parent, child, env, lambdaArgs[i]);
    }
    log.info("Leaving calculateEnv");
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
            try {
                env2->values[displ] = attrDef.e->maybeThunk(
                    *state, attrDef.inherited ? *up : *env2);
            } catch (Error& e) {
                env2->values[displ] = state->allocValue();
                env2->values[displ]->mkNull();
            }
            displ++;
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
    log.info("Entering calculateLambdaArgs");
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
        auto flakeDotNixPos = file.path.find("/flake.nix");
        if (flakeDotNixPos == std::string::npos) {
            return result;
        }
        auto flakeDir = file.path.substr(0, flakeDotNixPos);
        auto lockFilePath = flakeDir + "/flake.lock";
        log.info("lockFilePath: ", lockFilePath);
        auto lockFile = LockFile::read(lockFilePath);

        auto getFlakeInputs = allocRootValue(state->allocValue());
        state->eval(state->parseExprFromString(
                        // #include "flake/call-flake.nix.gen.hh"
                        R"(
# Modified from call-flake.nix
lockFileStr:

let

  lockFile = builtins.fromJSON lockFileStr;

  allNodes =
    builtins.mapAttrs
      (key: node:
        let

          sourceInfo =
            assert key != lockFile.root;
            fetchTree (node.info or {} // removeAttrs node.locked ["dir"]);

          subdir = if key == lockFile.root then rootSubdir else node.locked.dir or "";

          flake = import (sourceInfo + (if subdir != "" then "/" else "") + subdir + "/flake.nix");

          inputs = builtins.mapAttrs
            (inputName: inputSpec: allNodes.${resolveInput inputSpec})
            (node.inputs or {});

          # Resolve a input spec into a node name. An input spec is
          # either a node name, or a 'follows' path from the root
          # node.
          resolveInput = inputSpec:
              if builtins.isList inputSpec
              then getInputByPath lockFile.root inputSpec
              else inputSpec;

          # Follow an input path (e.g. ["dwarffs" "nixpkgs"]) from the
          # root node, returning the final node.
          getInputByPath = nodeName: path:
            if path == []
            then nodeName
            else
              getInputByPath
                # Since this could be a 'follows' input, call resolveInput.
                (resolveInput lockFile.nodes.${nodeName}.inputs.${builtins.head path})
                (builtins.tail path);

          outputs = flake.outputs (inputs // { self = result; });

          result = outputs // sourceInfo // { inherit inputs; inherit outputs; inherit sourceInfo; _type = "flake"; };
        in
          if node.flake or true then
            assert builtins.isFunction flake.outputs;
            result
          else
            sourceInfo
      )
      (builtins.removeAttrs lockFile.nodes [ lockFile.root ]);

in builtins.mapAttrs (key: value: allNodes.${key}) lockFile.nodes.${lockFile.root}.inputs
                            )",
                        "/"),
                    **getFlakeInputs);

        auto vLocks = state->allocValue();
        auto vRootSrc = state->allocValue();
        auto vRootSubdir = state->allocValue();
        auto vRes = state->allocValue();

        vLocks->mkString(lockFile.to_string());
        vRootSrc->mkAttrs(0);
        vRootSubdir->mkString("");

        try {
            state->callFunction(**getFlakeInputs, *vLocks, *vRes, noPos);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            return result;
        }

        stringstream ss;
        vRes->print(state->symbols, ss);
        log.info("Flake inputs: ", ss.str());

        auto root = dynamic_cast<ExprAttrs*>(exprPath.back());
        if (!root) {
            log.info("Flake does not start with attrs", ss.str());
            return result;
        }

        auto outputs = root->attrs.find(state->sOutputs);
        if (outputs == root->attrs.end()) {
            log.info("Flake does not contain `outputs`");
            return result;
        }

        if (exprPath.size() >= 2 &&
            exprPath[exprPath.size() - 2] == outputs->second.e) {
            result[exprPath.size() - 2] = vRes;
        }
    }

    log.info("Leaving calculateLambdaArgs");

    return result;
}

optional<Schema> NixAnalyzer::getSchema(Env& env, Expr* parent, Expr* child) {
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
        Value v;
        try {
            call->fun->eval(*state, env, v);
        } catch (Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            return {};
        }
        if (!v.isLambda()) {
            log.info("Trying to getSchema something that's not a lambda");
            return {};
        }
        if (v.lambda.fun->hasFormals()) {
            Schema schema;
            for (auto formal : v.lambda.fun->formals->formals) {
                schema.items.push_back({state->symbols[formal.name], ""});
            }
            return schema;
        }
    }
    return {};
}

Path FileInfo::nixpkgs() {
    return "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source";
}
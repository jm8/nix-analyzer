#include "nix-analyzer.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <variant>
#include "attr-set.hh"
#include "debug.h"

#include "error.hh"
#include "eval.hh"
#include "flake/flake.hh"
#include "flake/lockfile.hh"
#include "globals.hh"
#include "nixexpr.hh"
#include "pos.hh"
#include "schema.h"
#include "symbol-table.hh"
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
    Analysis analysis;
    state->parseWithCallback(
        source, path.empty() ? nix::foString : nix::foFile, path, basePath,
        state->staticBaseEnv,
        [&](auto x, Pos start, Pos end) {
            // fix
            // a.[cursor]b.c
            if (holds_alternative<CallbackAttrPath>(x)) {
                start.column -= 1;
            }
            if (start.origin != targetPos.origin ||
                start.file != targetPos.file) {
                return;
            }

            if (!(poscmp(start, targetPos) <= 0 &&
                  poscmp(targetPos, end) <= 0)) {
                return;
            }

            if (holds_alternative<Expr*>(x)) {
                auto e = get<Expr*>(x);
                if (auto path = dynamic_cast<ExprPath*>(e)) {
                    analysis.paths.push_back({path, start, end});
                }
                analysis.exprPath.push_back(e);
            } else if (holds_alternative<CallbackAttrPath>(x)) {
                if (analysis.attr) {
                    log.warning("overwriting attr of exprpath");
                }
                analysis.attr = {get<CallbackAttrPath>(x)};
            } else if (holds_alternative<CallbackFormal>(x)) {
                if (analysis.formal) {
                    log.warning("overwriting formal of exprpath");
                }
                analysis.formal = get<CallbackFormal>(x).formal;
            } else if (holds_alternative<CallbackInherit>(x)) {
                analysis.inherit = {get<CallbackInherit>(x).expr};
            }
        },
        [&analysis](ParseError error) {
            analysis.parseErrors.push_back(error);
        });
    return analysis;
}

pair<NACompletionType, vector<NACompletionItem>> NixAnalyzer::complete(
    const Analysis& analysis,
    FileInfo file) {
    const auto& exprPath = analysis.exprPath;
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
    log.info("file.type: ", (int)file.type);

    if (auto select = dynamic_cast<ExprSelect*>(exprPath.front())) {
        size_t howManyAttrsToKeep;
        if (analysis.attr) {
            howManyAttrsToKeep = analysis.attr->index;
        } else {
            log.warning("no attr in analysis when completing ExprSelect");
            return {};
        }
        AttrPath path(select->attrPath.begin(),
                      select->attrPath.begin() + howManyAttrsToKeep);
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

    if (dynamic_cast<ExprAttrs*>(exprPath.front())) {
        optional<Schema> schema = getSchema(*env, exprPath, file);
        // if cursor is inside inherit we want to fall back to variable
        // completion
        if (!analysis.inherit) {
            if (!schema) {
                log.info("Don't know the schema of this attrs.");
                return {};
            }
            AttrPath path;
            if (analysis.attr) {
                size_t howManyAttrsToKeep = analysis.attr->index;
                path = AttrPath(
                    analysis.attr->attrPath->begin(),
                    analysis.attr->attrPath->begin() + howManyAttrsToKeep);
            } else {
                path = AttrPath();
            }

            for (auto attrName : path) {
                if (!attrName.symbol) {
                    log.info("Encountered a dynamic index");
                    return {};
                }
                schema = schema->subschema(*state, attrName.symbol);
                if (!schema) {
                    log.info("No subschema");
                    return {};
                }
            }
            return {NACompletionType::Field, schema->getItems(*state)};
        }
        // this is an inherit (expr) ...;
        if (analysis.inherit.has_value() &&
            analysis.inherit.value().has_value()) {
            Expr* e = analysis.inherit->value();
            Value v;
            try {
                e->eval(*state, *env, v);
                state->forceAttrs(v, noPos);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                v.mkAttrs(state->allocBindings(0));
            }
            vector<NACompletionItem> result;
            for (auto attr : *v.attrs) {
                result.push_back({state->symbols[attr.name]});
            }
            return {NACompletionType::Property, result};
        }
    }

    if (dynamic_cast<ExprLambda*>(exprPath.front())) {
        if (!lambdaArgs.front()) {
            log.info("completion of unknown lambda arg");
            return {};
        }
        Value* lambdaArg = *lambdaArgs.front();
        if (lambdaArg->type() != nAttrs) {
            log.info("completion of lambda arg that is not attrs");
            return {};
        }
        vector<NACompletionItem> result;
        for (auto attr : *lambdaArg->attrs) {
            result.push_back({state->symbols[attr.name]});
        }
        return {NACompletionType::Variable, result};
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

NAHoverResult NixAnalyzer::hover(const Analysis& analysis, FileInfo file) {
    const auto& exprPath = analysis.exprPath;
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
        size_t howManyAttrsToKeep;
        if (analysis.attr) {
            howManyAttrsToKeep = analysis.attr->index + 1;
        } else {
            log.warning("no attr in analysis when completing ExprSelect");
            return {};
        }
        AttrPath path(select->attrPath.begin(),
                      select->attrPath.begin() + howManyAttrsToKeep);
        ExprSelect prefix(select->pos, select->e, path, select->def);
        Value v;
        cerr << "\n";
        try {
            prefix.eval(*state, *env, v);
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
    if (dynamic_cast<ExprLambda*>(exprPath.front())) {
        if (analysis.formal) {
            if (!lambdaArgs.front()) {
                log.info("hover of lambda with unknown args");
                return {};
            }
            Value* lambdaArg = *lambdaArgs.front();
            if (lambdaArg->type() != nAttrs) {
                log.info("lambda arg is not an attrset");
                return {};
            }
            auto it = lambdaArg->attrs->find(analysis.formal->name);
            Value* v;
            if (it == lambdaArg->attrs->end()) {
                log.info("lambda arg does not contain this attr");
                v = state->allocValue();
                v->mkNull();
            } else {
                v = it->value;
            }
            try {
                state->forceValue(*v, noPos);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                v->mkNull();
            }
            PosIdx posIdx = v->definitionPos;
            if (posIdx) {
                return {{stringifyValue(*state, *v)},
                        {state->positions[posIdx]}};
            } else {
                log.info("Pos doesn't exist.");
                return {{stringifyValue(*state, *v)}, {}};
            }
        }
    }
    // inherit x; or inherit (expr) x;. just look up the value on the attrset xd
    if (analysis.inherit.has_value()) {
        if (!analysis.attr)
            return {};
        auto [index, attrPath] = *analysis.attr;
        auto name = (*attrPath)[index];
        if (!name.symbol) {
            log.info("dynamic attribute in inherit is not allowed");
            return {};
        }
        Value v;

        ExprSelect select(noPos, exprPath.front(), name.symbol);
        try {
            select.eval(*state, *env, v);
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
    // these are attrpaths that are not inherited
    if (analysis.attr) {
        const auto& attrPath = analysis.attr->attrPath;
        if (auto let = dynamic_cast<ExprLet*>(exprPath.front())) {
            if (attrPath->size() != 1) {
                log.info("didn't know you could do let a.b.c = ...;");
                return {};
            }
            auto name = attrPath->back();
            if (!name.symbol) {
                log.info("didn't know you could do let ${x} = ...;");
                return {};
            }
            auto it = let->attrs->attrs.find(name.symbol);
            if (it == let->attrs->attrs.end()) {
                log.warning("can't find the let binding that you made");
                return {};
            }
            auto child = it->second.e;
            auto subEnv = updateEnv(let, child, env, {});
            Value v;
            try {
                it->second.e->eval(*state, *subEnv, v);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                return {};
            }
            PosIdx posIdx = v.definitionPos;
            if (posIdx) {
                return {{stringifyValue(*state, v)},
                        {state->positions[posIdx]}};
            } else {
                log.info("Pos doesn't exist.");
                return {{stringifyValue(*state, v)}, {}};
            }
        }
    }

    return {};
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
            try {
                env2->values[displ] = attrDef.e->maybeThunk(
                    *state, attrDef.inherited ? *up : *env2);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                env2->values[displ] = state->allocValue();
                env2->values[displ]->mkNull();
            }
            env2->values[displ]->definitionPos = attrDef.pos;
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
            env2->values[displ] = vAttr;
            env2->values[displ]->definitionPos = i.second.pos;
            displ++;
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

    return result;
}

optional<pair<size_t, Schema>>
NixAnalyzer::getSchemaRoot(Env& env, vector<Expr*> exprPath, FileInfo file) {
    if (exprPath.empty()) {
        return {};
    }

    Env* functionEnv = &env;
    for (size_t i = 0; i < exprPath.size(); i++) {
        if (auto attrs = dynamic_cast<ExprAttrs*>(exprPath[i])) {
            if (attrs->recursive) {
                functionEnv = functionEnv->up;
            }
        } else if (auto call = dynamic_cast<ExprCall*>(exprPath[i])) {
            if (i >= 1 && exprPath[i - 1] == call->fun) {
                return {};
            }
            ExprSelect* select = dynamic_cast<ExprSelect*>(call->fun);
            ExprVar* var = dynamic_cast<ExprVar*>(call->fun);
            if ((select && state->symbols[select->attrPath.back().symbol] ==
                               "mkDerivation") ||
                (var && state->symbols[var->name] == "mkDerivation")) {
                log.info("Completing with schemaMkDerivation");
                return {{i - 1, schemaMkDerivation}};
            }
            Value v;
            try {
                call->fun->eval(*state, *functionEnv, v);
            } catch (Error& e) {
                log.info("Caught error: ", e.info().msg.str());
                return {};
            }

            if (v.isLambda()) {
                if (v.lambda.fun->hasFormals()) {
                    vector<NACompletionItem> schema;
                    for (auto formal : v.lambda.fun->formals->formals) {
                        schema.push_back({state->symbols[formal.name], ""});
                    }
                    return {{i - 1, schema}};
                }
            } else if (v.type() == nAttrs) {
                auto it =
                    v.attrs->find(state->symbols.create("__functionArgs"));
                if (it == v.attrs->end()) {
                    log.info(
                        "tried to getSchema something thats not a function");
                    return {};
                }
                try {
                    state->forceAttrs(*it->value, noPos);
                } catch (Error& e) {
                    log.info("Caught error: ", e.info().msg.str());
                    log.info("for some reason __functionArgs isn't a set");
                    return {};
                }
                vector<NACompletionItem> schema;
                for (auto attr : *it->value->attrs) {
                    schema.push_back({state->symbols[attr.name], ""});
                }
                return {{i - 1, schema}};
            } else {
                log.info("tried to getSchema something thats not a function");
                return {};
            }
        } else {
            log.info(
                "Encountered something that's not Attrs or Call, so stopping "
                "the get function schema");
            break;
        }
    }

    if (file.type == FileType::NixosModule) {
        try {
            Value* evalConfigFunction = state->allocValue();
            Value* evalConfigArg = state->allocValue();
            Value* system = state->allocValue();
            system->mkString(settings.thisSystem.get());
            Value* modules = state->allocValue();
            modules->mkList(0);
            state->evalFile(file.nixpkgs() + "/nixos/lib/eval-config.nix",
                            *evalConfigFunction);
            evalConfigArg->mkAttrs(state->allocBindings(2));
            Attr x;
            evalConfigArg->attrs->push_back(Attr(state->sSystem, system));
            evalConfigArg->attrs->push_back(
                Attr(state->symbols.create("modules"), modules));
            Value* module = state->allocValue();
            state->callFunction(*evalConfigFunction, *evalConfigArg, *module,
                                noPos);
            state->forceAttrs(*module, noPos);
            auto optionsAttr =
                module->attrs->get(state->symbols.create("options"));
            if (!optionsAttr) {
                log.warning(
                    "eval-config.nix did not give something with 'options'");
                return {};
            }
            return {{exprPath.size() - 1, optionsAttr->value}};
        } catch (nix::Error& e) {
            log.info("Caught error: ", e.info().msg.str());
            return {};
        }
    }
    return {};
}

optional<Schema> NixAnalyzer::getSchema(Env& env,
                                        vector<Expr*> exprPath,
                                        FileInfo file) {
    auto optRootSchema = getSchemaRoot(env, exprPath, file);
    if (!optRootSchema) {
        return {};
    }
    auto [rootIndex, rootSchema] = *optRootSchema;

    Schema currentSchema = move(rootSchema);

    for (size_t i = rootIndex; i >= 1; i--) {
        Expr* parent = exprPath[i];
        Expr* child = exprPath[i - 1];
        if (auto attrs = dynamic_cast<ExprAttrs*>(parent)) {
            // todo: store the symbol as part of the exprpath
            optional<Symbol> subname;
            for (auto [symbol, attrDef] : attrs->attrs) {
                if (!attrDef.inherited && attrDef.e == child) {
                    subname = symbol;
                }
            }
            if (!subname) {
                log.info(
                    "Failed to find the child in the parent attrs so don't "
                    "know the symbol");
                return {};
            }
            auto subschema = currentSchema.subschema(*state, *subname);
            if (!subschema) {
                log.info("No subschema");
                return {};
            }
            currentSchema = *subschema;
        } else {
            return {};
        }
    }
    return currentSchema;
}

FileInfo::FileInfo() : path(""), type(FileType::None) {
}

FileInfo::FileInfo(Path path) : path(path) {
    if (path.find("flake.nix") != string::npos) {
        type = FileType::Flake;
    } else if (path.find("configuration.nix") != string::npos) {
        type = FileType::NixosModule;
    } else {
        type = FileType::Package;
    }
}

FileInfo::FileInfo(Path path, FileType ftype) : path(path), type(ftype) {
}

Path getDefaultNixpkgs() {
    return "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source";
}

Path FileInfo::nixpkgs() {
    auto it = path.find("/pkgs/");
    if (it != string::npos) {
        auto prefix = path.substr(0, it);
        // todo: check if it's sensible
        return prefix;
    }
    return getDefaultNixpkgs();
}
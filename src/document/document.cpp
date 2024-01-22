#include "document/document.h"
#include <nix/eval.hh>
#include <iostream>
#include "input-accessor.hh"
#include "nixexpr.hh"
#include "parser/parser.h"

Document::Document(
    nix::EvalState& state,
    nix::SourcePath path,
    std::string source
)
    : state(state), path(path), source(source) {}

void Document::_parse() {
    if (root.has_value())
        return;
    auto result = parse(state, path, source);
    tokens = result.tokens;
    root = result.root;
    parseErrors = result.parseErrors;
    for (auto& it : result.exprData) {
        exprData[it.first] = {it.second.range};
    }
    bindVars(state.staticBaseEnv, *root);
}

nix::Expr* Document::getRoot() {
    _parse();
    return *root;
}

Range Document::tokenRangeToRange(TokenRange tokenRange) {
    return {
        tokens[tokenRange.start].range.start, tokens[tokenRange.end].range.end};
}

std::shared_ptr<nix::StaticEnv> Document::getStaticEnv(nix::Expr* e) {
    _parse();
    return exprData[e].staticEnv;
}

std::optional<nix::Expr*> Document::getParent(nix::Expr* e) {
    _parse();
    return exprData[e].parent;
}

nix::Env* Document::getEnv(nix::Expr* e) {
    _parse();
    const auto& data = exprData[e];
    if (data.env) {
        return *data.env;
    }
    if (!data.parent) {
        return &state.baseEnv;
    }
    return updateEnv(*data.parent, e, getEnv(*data.parent));
}

nix::Value* Document::thunk(nix::Expr* e, nix::Env* env) {
    exprData[e].env = env;
    auto v = e->maybeThunk(state, *env);
    exprData[e].v = v;
    return v;
}
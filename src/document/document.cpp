#include "document/document.h"
#include "eval/calculateenv.h"
#include "input-accessor.hh"
#include "parser/parser.h"

Range Document::tokenRangeToRange(TokenRange tokenRange) {
    return {
        tokens[tokenRange.start].range.start, tokens[tokenRange.end].range.end};
}

std::shared_ptr<nix::StaticEnv> Document::getStaticEnv(nix::Expr* e) {
    return exprData[e].staticEnv;
}

nix::Env* Document::getEnv(nix::Expr* e) {
    const auto& data = exprData[e];
    if (data.env) {
        return *data.env;
    }
    if (!data.parent) {
        return &state.baseEnv;
    }
    return updateEnv(*this, *data.parent, e, getEnv(*data.parent));
}

nix::Value* Document::thunk(nix::Expr* e, nix::Env* env) {
    exprData[e].env = env;
    auto v = e->maybeThunk(state, *env);
    exprData[e].v = v;
    return v;
}
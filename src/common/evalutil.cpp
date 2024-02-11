#include "evalutil.h"
#include <nix/eval.hh>
#include <cstdlib>
#include <iostream>
#include "common/logging.h"
#include "nixexpr.hh"
#include "value.hh"

nix::Value* loadFile(nix::EvalState& state, std::string path) {
    auto v = state.allocValue();
    try {
        state.evalFile(std::string{RESOURCEPATH} + "/" + path, *v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        v->mkNull();
    }
    return v;
}

nix::Value* makeAttrs(
    nix::EvalState& state,
    std::vector<std::pair<std::string_view, nix::Value*>> binds
) {
    auto bindings = state.buildBindings(binds.size());
    for (auto [a, b] : binds) {
        bindings.insert(state.symbols.create(a), b);
    }
    auto result = state.allocValue();
    result->mkAttrs(bindings.finish());
    return result;
}

static nix::Value* _nixpkgsValue = nullptr;

nix::Value* nixpkgsValue(nix::EvalState& state) {
    if (_nixpkgsValue == nullptr) {
        auto pkgsFunction = state.allocValue();
        auto searchPath = state.getSearchPath();
        auto it = std::find_if(
            searchPath.begin(),
            searchPath.end(),
            [](nix::SearchPathElem& x) { return x.first == "nixpkgs"; }
        );
        if (it == searchPath.end()) {
            std::cerr << "Failed to find nixpkgs on search path\n";
            abort();
        }
        auto [ok, path] = state.resolveSearchPathElem(*it);
        if (!ok) {
            std::cerr << "Failed to find nixpkgs on search path\n";
            abort();
        }
        state.evalFile(path, *pkgsFunction);

        auto arg = state.allocValue();
        arg->mkAttrs(state.allocBindings(0));

        auto pkgs = state.allocValue();
        state.callFunction(*pkgsFunction, *arg, *pkgs, nix::noPos);

        _nixpkgsValue = pkgs;
    }
    return _nixpkgsValue;
}

std::optional<nix::Value*> getAttr(
    nix::EvalState& state,
    nix::Value* v,
    nix::Symbol symbol
) {
    try {
        state.forceAttrs(*v, nix::noPos);
    } catch (nix::Error &e) {
        REPORT_ERROR(e);
        return {};
    }
    auto it = v->attrs->get(symbol);
    if (!it)
        return {};
    return it->value;
}

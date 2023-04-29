#include "evalutil.h"
#include <nix/eval.hh>
#include "common/logging.h"

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
        state.evalFile(
            "/nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source", *pkgsFunction
        );

        auto arg = state.allocValue();
        arg->mkAttrs(state.allocBindings(0));

        auto pkgs = state.allocValue();
        state.callFunction(*pkgsFunction, *arg, *pkgs, nix::noPos);

        _nixpkgsValue = pkgs;
    }
    return _nixpkgsValue;
}
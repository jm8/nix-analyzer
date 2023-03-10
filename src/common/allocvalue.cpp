#include <nix/eval.hh>
#include <nix/value.hh>

nix::Value* allocValue(nix::EvalState& state) {
    // this causes segfault ¯\_(ツ)_/¯
    // return state.allocValue();

    return new nix::Value;
}
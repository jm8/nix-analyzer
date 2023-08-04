#pragma once
#include "na_config.h"
#include <nix/eval.hh>
#include <nix/value.hh>

nix::Value* loadFile(nix::EvalState& state, std::string path);
nix::Value* makeAttrs(
    nix::EvalState& state,
    std::vector<std::pair<std::string_view, nix::Value*>> binds
);
nix::Value* nixpkgsValue(nix::EvalState& state);
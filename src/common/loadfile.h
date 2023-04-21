#pragma once
#include "config.h"
#include <nix/eval.hh>
#include <nix/value.hh>

nix::Value* loadFile(nix::EvalState& state, std::string path);
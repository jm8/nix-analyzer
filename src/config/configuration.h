#pragma once
#include "na_config.h"
#include <string>
#include "eval.hh"
#include "value.hh"
#include "common/evalutil.h"

struct Ftype;

Ftype ftypeFromValue(nix::EvalState &state, nix::Value*);

class Config {
    // goes in order from most specific to least specific configuration file
    std::vector<nix::Value*> values;

   public:
    static Config load(nix::EvalState& state, std::string path);

    Ftype get_ftype(nix::EvalState &state, std::string_view path);
};

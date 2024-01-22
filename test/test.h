#include <catch2/catch.hpp>
#include "eval.hh"

extern std::shared_ptr<nix::EvalState> stateptr;

nix::SourcePath path(std::string p);
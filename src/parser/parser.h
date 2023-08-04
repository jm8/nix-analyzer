#pragma once
#include "na_config.h"
#include <nix/nixexpr.hh>
#include <memory>
#include <string>
#include <vector>
#include "common/analysis.h"
#include "common/position.h"

Analysis parse(
    nix::EvalState& state,
    std::string source,
    nix::Path path,
    nix::Path basePath,
    Position targetPos
);
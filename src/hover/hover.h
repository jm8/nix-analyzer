#pragma once
#include "na_config.h"
#include <optional>
#include <string>
#include "common/position.h"

struct Analysis;

struct HoverResult {
    std::string markdown;
    std::optional<Location> definitionPos;
};

std::optional<HoverResult> hover(nix::EvalState& state, Analysis& analysis);
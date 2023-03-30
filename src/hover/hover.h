#include "config.h"
#include <optional>
#include <string>
#include "common/analysis.h"
#include "common/position.h"

struct HoverResult {
    std::string text;
};

std::optional<HoverResult> hover(nix::EvalState& state, Analysis& analysis);
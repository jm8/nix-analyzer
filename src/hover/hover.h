#include "config.h"
#include <optional>
#include <string>
#include "common/analysis.h"
#include "common/position.h"

std::optional<std::string> hover(nix::EvalState& state, Analysis& analysis);
#include "na_config.h"
#include <optional>
#include <string>
#include "common/analysis.h"
#include "common/position.h"

struct CompletionResult {
    std::vector<std::string> items;
};

CompletionResult completion(nix::EvalState& state, Analysis& analysis);
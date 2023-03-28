#include "hover.h"
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
std::optional<std::string> hover(nix::EvalState& state, Analysis& analysis) {
    if (analysis.attr) {
        auto& attrPath = *analysis.attr->attrPath;
        return state.symbols[attrPath[analysis.attr->index].symbol];
    }
    return {};
}
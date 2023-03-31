#include "hover.h"
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include <iostream>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/logging.h"
#include "common/stringify.h"

std::optional<HoverResult> hoverSelect(
    nix::EvalState& state,
    Analysis& analysis
) {
    auto select = dynamic_cast<nix::ExprSelect*>(analysis.exprPath.front().e);
    if (!select) {
        return {};
    }
    if (!analysis.attr) {
        std::cerr << "hover of select without analysis.attr\n";
        return {};
    }
    auto prefixPath = select->attrPath;
    prefixPath.erase(
        prefixPath.begin() + analysis.attr->index + 1, prefixPath.end()
    );
    auto prefix =
        prefixPath.size() > 0
            ? new nix::ExprSelect(nix::noPos, select->e, prefixPath, nullptr)
            : select->e;
    std::cerr << stringify(state, prefix) << "\n";
    nix::Value v;
    try {
        auto env = analysis.exprPath.front().env;
        std::cerr << env << "\n";
        prefix->eval(state, *analysis.exprPath.front().env, v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    return {{stringify(state, &v)}};
}

std::optional<HoverResult> hover(nix::EvalState& state, Analysis& analysis) {
    return hoverSelect(state, analysis);
}
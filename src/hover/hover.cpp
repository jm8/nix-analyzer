#include "hover.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include <iostream>
#include <optional>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/logging.h"
#include "common/stringify.h"
#include "schema/schema.h"

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
        prefix->eval(state, *analysis.exprPath.front().env, v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    return {{stringify(state, &v)}};
}

std::optional<HoverResult> hoverAttr(
    nix::EvalState& state,
    Analysis& analysis
) {
    auto attrs = dynamic_cast<nix::ExprAttrs*>(analysis.exprPath.front().e);
    if (!attrs) {
        return {};
    }
    if (!analysis.attr) {
        std::cerr << "hover of attrs without analysis.attr\n";
        return {};
    }
    auto prefixPath = analysis.attr->attrPath;
    prefixPath->erase(
        prefixPath->begin() + analysis.attr->index + 1, prefixPath->end()
    );
    Schema schema = getSchema(state, analysis);
    for (auto attr : *prefixPath) {
        schema = schema.attrSubschema(state, attr.symbol);
    }
    return schema.hover(state);
}

std::optional<HoverResult> hoverVar(nix::EvalState& state, Analysis& analysis) {
    auto var = dynamic_cast<nix::ExprVar*>(analysis.exprPath.front().e);
    if (!var) {
        return {};
    }
    nix::Value v;
    try {
        auto env = analysis.exprPath.front().env;
        var->eval(state, *analysis.exprPath.front().env, v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    return {{stringify(state, &v)}};
}

std::optional<HoverResult> hover(nix::EvalState& state, Analysis& analysis) {
    std::optional<HoverResult> result;
    if (!result)
        result = hoverSelect(state, analysis);
    if (!result)
        result = hoverAttr(state, analysis);
    if (!result)
        result = hoverVar(state, analysis);
    return result;
}
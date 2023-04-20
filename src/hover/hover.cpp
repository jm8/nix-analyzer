#include "hover.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/value.hh>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/logging.h"
#include "common/stringify.h"
#include "schema/schema.h"

HoverResult hoverPrimop(nix::Value* v) {
    assert(v->isPrimOp());
    std::stringstream ss;
    ss << "### built-in function `" << v->primOp->name << "`";
    if (!v->primOp->args.empty()) {
        ss << " *`";
        for (const auto& arg : v->primOp->args) {
            ss << arg << " ";
        }
        ss << "`*";
    }
    ss << "\n\n";
    if (v->primOp->doc) {
        bool isStartOfLine = true;
        std::string_view doc{v->primOp->doc};
        int spacesToRemoveAtStartOfLine = 6;
        for (char c : doc) {
            if (c == '\n') {
                ss << '\n';
                spacesToRemoveAtStartOfLine = 6;
            } else if (spacesToRemoveAtStartOfLine > 0 && c == ' ') {
                spacesToRemoveAtStartOfLine--;
            } else {
                ss << c;
                spacesToRemoveAtStartOfLine = -1;
            }
        }
    }
    return {ss.str()};
}

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
    auto name = prefixPath.back().symbol;
    if (!name) {
        std::cerr << "hover of non-symbol select attr";
        return {};
    }
    auto prefix =
        prefixPath.size() > 0
            ? new nix::ExprSelect(nix::noPos, select->e, prefixPath, nullptr)
            : select->e;
    std::cerr << stringify(state, prefix) << "\n";
    auto v = state.allocValue();
    try {
        auto env = analysis.exprPath.front().env;
        prefix->eval(state, *analysis.exprPath.front().env, *v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    if (v->isPrimOp()) {
        return hoverPrimop(v);
    }
    return {};
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

std::optional<size_t> getExprForLevel(
    const Analysis& analysis,
    nix::Level targetLevel
) {
    nix::Level currLevel = 0;
    int j;
    for (j = 0; j < analysis.exprPath.size() - 1; j++) {
        if (analysis.exprPath[j].env != analysis.exprPath[j + 1].env) {
            const auto& child = analysis.exprPath[j];
            const auto& parent = analysis.exprPath[j + 1];
            if (currLevel == targetLevel) {
                return j + 1;
            }
            currLevel++;
        }
    }
    return {};
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
    if (v.isPrimOp()) {
        return hoverPrimop(&v);
    }
    auto j = getExprForLevel(analysis, var->level);
    if (!j) {
        return {};
    }
    std::stringstream ss;
    auto e = analysis.exprPath[*j].e;
    ss << "```nix\n";
    e->show(state.symbols, ss);
    ss << "\n```";

    return {{ss.str()}};
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
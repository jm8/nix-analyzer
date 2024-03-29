#include "hover.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/evalutil.h"
#include "common/logging.h"
#include "common/position.h"
#include "common/stringify.h"
#include "schema/schema.h"

std::string documentationPrimop(nix::EvalState& state, nix::Value* v) {
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
    return ss.str();
}

std::string documentationDerivation(nix::EvalState& state, nix::Value* v) {
    assert(state.isDerivation(*v));
    nix::Value* vFunction = loadFile(state, "hover/getDerivationDoc.nix");
    nix::Value* vRes = state.allocValue();
    try {
        state.callFunction(*vFunction, *v, *vRes, nix::noPos);
        return std::string{state.forceString(*vRes)};
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return "";
    }
}

std::string getUriSource(const Analysis& analysis, std::string_view uri) {
    std::string_view prefix = "file://";
    if (uri.starts_with(prefix)) {
        auto path = uri.substr(prefix.size());
        if (path == analysis.path) {
            return analysis.source;
        }
        return nix::readFile(std::string{path});
    }
    return "";
}

std::string documentationComment(
    const Analysis& analysis,
    const Location& loc
) {
    std::string source = getUriSource(analysis, loc.uri);
    std::istringstream ss{source};
    std::string line;
    int linenum = 0;
    std::stringstream currentComment;
    auto getLineComment = [](std::string_view s
                          ) -> std::optional<std::string_view> {
        auto a = s.find_first_not_of(" \t");
        if (a == std::string_view::npos) {
            return "";
        }
        if (s[a] != '#') {
            return {};
        }
        auto b = s.find_first_not_of(" \t", a + 1);
        if (b == std::string_view::npos) {
            return "";
        }
        return s.substr(b);
    };
    while (std::getline(ss, line)) {
        if (linenum == loc.range.start.line) {
            return currentComment.str();
        }
        if (auto lineComment = getLineComment(line)) {
            currentComment << *lineComment << "\n";
        } else {
            currentComment.str("");
        }
        linenum++;
    }
    return "";
}

std::string documentationLambda(
    nix::EvalState& state,
    const Analysis& analysis,
    nix::Value* v
) {
    assert(v->isLambda());

    nix::ExprLambda* lambda = v->lambda.fun;
    Location loc{state.positions[v->lambda.fun->pos]};
    std::stringstream ss;
    ss << "### lambda `" << state.symbols[lambda->name] << "` *`";
    if (lambda->hasFormals()) {
        auto sep = "";
        ss << "{ ";
        for (auto formal : lambda->formals->formals) {
            ss << sep << state.symbols[formal.name];
            ss << "?";
            sep = ", ";
        }
        if (lambda->formals->ellipsis) {
            ss << sep << "...";
        }
        ss << " }";
    } else {
        nix::ExprLambda* curr = lambda;
        auto sep = "";
        while (curr) {
            ss << sep << state.symbols[curr->arg];
            curr = dynamic_cast<nix::ExprLambda*>(curr->body);
            sep = " ";
        }
    }
    ss << "`*\n\n";
    ss << documentationComment(analysis, state.positions[v->lambda.fun->pos]);
    return ss.str();
}

void printValue(
    std::ostream& stream,
    nix::EvalState& state,
    nix::Value* v,
    int indentation = 0,
    int maxDepth = 2
) {
    try {
        state.forceValue(*v, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        stream << "[error]";
        return;
    }
    std::string spaces(indentation, ' ');
    switch (v->type()) {
        case nix::nAttrs:
            if (state.isDerivation(*v)) {
                stream << "<DERIVATION>";
                break;
            }
            if (maxDepth > 0) {
                stream << "{\n";
                int n = 0;
                for (auto& i : *v->attrs) {
                    stream << spaces << "  ";
                    if (n > 20) {
                        stream << "/* ... */ \n";
                        break;
                    }
                    stream << state.symbols[i.name] << " = ";
                    printValue(
                        stream, state, i.value, indentation + 2, maxDepth - 1
                    );
                    stream << ";\n";
                    n++;
                }

                stream << spaces << "}";
            } else {
                stream << "{ /* ... */ }";
            }
            break;
        case nix::nList:
            if (maxDepth > 0) {
                stream << "[\n";
                int n = 0;
                for (auto& i : v->listItems()) {
                    stream << spaces << "  ";
                    if (n > 20) {
                        stream << "/* ... */ \n";
                        break;
                    }
                    printValue(stream, state, i, indentation + 2, maxDepth - 1);
                    stream << "\n";
                    n++;
                }

                stream << spaces << "]";
            } else {
                stream << "[ /* ... */ ]";
            }
            break;
        default:
            v->print(state.symbols, stream);
            break;
    }
}

std::string valueType(nix::Value* v) {
    switch (v->type()) {
        case nix::nThunk:
            return "thunk";
        case nix::nInt:
            return "integer";
        case nix::nFloat:
            return "float";
        case nix::nBool:
            return "boolean";
        case nix::nString:
            return "string";
        case nix::nPath:
            return "path";
        case nix::nNull:
            return "null";
        case nix::nAttrs:
            return "attrset";
        case nix::nList:
            return "list";
        case nix::nFunction:
            return "function";
        case nix::nExternal:
            return "external";
    }
}

std::string documentationValue(
    const Analysis& analysis,
    nix::EvalState& state,
    nix::Value* v
) {
    try {
        state.forceValue(*v, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return "";
    }
    if (v->isPrimOp()) {
        return documentationPrimop(state, v);
    }
    if (state.isDerivation(*v)) {
        return documentationDerivation(state, v);
    }
    if (v->isLambda()) {
        return documentationLambda(state, analysis, v);
    }

    std::stringstream ss;
    ss << "### " << valueType(v) << "\n\n";
    ss << "```nix\n";
    printValue(ss, state, v);
    ss << "\n```";

    return ss.str();
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
        prefixPath.begin() + analysis.attr->index, prefixPath.end()
    );
    auto name = select->attrPath[analysis.attr->index].symbol;
    if (!name) {
        std::cerr << "hover of non-symbol select attr\n";
        return {};
    }
    auto prefix =
        prefixPath.size() > 0
            ? new nix::ExprSelect(nix::noPos, select->e, prefixPath, nullptr)
            : select->e;
    auto vAttrs = state.allocValue();
    try {
        auto env = analysis.exprPath.front().env;
        prefix->eval(state, *analysis.exprPath.front().env, *vAttrs);
        state.forceAttrs(*vAttrs, nix::noPos);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    auto attr = vAttrs->attrs->get(name);
    if (!attr) {
        return {};
    }
    HoverResult result;
    if (attr->pos) {
        Location loc{state.positions[attr->pos]};
        result.definitionPos = loc;
    }
    // result.markdown = ss.str();
    result.markdown += documentationValue(analysis, state, attr->value);
    return result;
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
    nix::Value* v = state.allocValue();
    try {
        auto env = analysis.exprPath.front().env;
        var->eval(state, *analysis.exprPath.front().env, *v);
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    auto j = getExprForLevel(analysis, var->level);
    if (j) {
        auto e = analysis.exprPath[*j].e;
        if (auto let = dynamic_cast<nix::ExprLet*>(e)) {
            auto attr = let->attrs->attrs.find(var->name);
            if (attr == let->attrs->attrs.end()) {
                std::cerr << "didn't find the attr in let";
                return {};
            }
            Location loc = state.positions[attr->second.pos];
            return {{documentationValue(analysis, state, v), loc}};
        }
        if (auto with = dynamic_cast<nix::ExprWith*>(e)) {
            auto withChildEnv = analysis.exprPath[*j - 1].env;
            if (withChildEnv->type != nix::Env::HasWithAttrs) {
                std::cerr
                    << "the env of this with should be HasWithAttrs because "
                       "the expression has been evaluated\n";
                return {};
            }
            nix::Value* attrs = *withChildEnv->values;
            if (attrs->type() != nix::nAttrs) {
                std::cerr << "the with expression should be attrs\n";
                return {};
            }
            auto attr = attrs->attrs->find(var->name);
            Location loc = state.positions[attr->pos];
            std::stringstream ss;
            ss << documentationValue(analysis, state, v);
            ss << "\n\n---\n\n```nix\n";
            // ss << "# line " << state.positions[with->pos].line << "\n";
            ss << "with ";
            with->attrs->show(state.symbols, ss);
            ss << "; /* ... */ \n```";
            return {{ss.str(), loc}};
        }
    }
    return {{documentationValue(analysis, state, v), {}}};
}

std::optional<HoverResult> hoverFormal(
    nix::EvalState& state,
    Analysis& analysis
) {
    if (!analysis.formal) {
        return {};
    }

    if (!analysis.exprPath.front().lambdaArg) {
        return {};
    }
    auto lambdaArg = *analysis.exprPath.front().lambdaArg;
    auto attr = lambdaArg->attrs->get(analysis.formal->name);
    if (!attr) {
        return {};
    }

    return {{documentationValue(analysis, state, attr->value)}};
}

std::optional<HoverResult> hoverArg(nix::EvalState& state, Analysis& analysis) {
    if (!analysis.arg) {
        return {};
    }

    if (!analysis.exprPath.front().lambdaArg) {
        return {};
    }
    auto lambdaArg = *analysis.exprPath.front().lambdaArg;

    return {{documentationValue(analysis, state, lambdaArg)}};
}

std::optional<HoverResult> hoverInherit(
    nix::EvalState& state,
    Analysis& analysis
) {
    if (!analysis.inherit)
        return {};

    if (auto attrs =
            dynamic_cast<nix::ExprAttrs*>(analysis.exprPath.front().e)) {
        auto it = attrs->attrs.find(analysis.inherit->symbol);
        if (it == attrs->attrs.end()) {
            return {};
        }
        analysis.exprPath.insert(
            analysis.exprPath.begin(),
            ExprPathItem{it->second.e, analysis.exprPath.front().env, {}}
        );
        if (analysis.inherit->e) {
            analysis.attr = {0, new nix::AttrPath{analysis.inherit->symbol}};
        }
        analysis.inherit = {};
        auto result = hover(state, analysis);
        return result;
    }
    return {};
}

std::optional<HoverResult> hover(nix::EvalState& state, Analysis& analysis) {
    std::optional<HoverResult> result;
    if (!result)
        result = hoverInherit(state, analysis);
    if (!result)
        result = hoverSelect(state, analysis);
    if (!result)
        result = hoverAttr(state, analysis);
    if (!result)
        result = hoverFormal(state, analysis);
    if (!result)
        result = hoverArg(state, analysis);
    if (!result)
        result = hoverVar(state, analysis);
    return result;
}
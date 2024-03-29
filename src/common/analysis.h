#pragma once
#include "na_config.h"
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <nlohmann/json.hpp>
#include <vector>
#include "common/document.h"
#include "common/position.h"
#include "schema/schema.h"

struct Diagnostic {
    std::string message;
    Range range;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Diagnostic, message, range);

struct ParseResultAttrPath {
    size_t index;
    nix::AttrPath* attrPath;
};

struct ParseResultInherit {
    nix::Symbol symbol;
    std::optional<nix::Expr*> e;
};

struct ExprPathItem {
    nix::Expr* e;
    nix::Env* env;
    std::optional<nix::Value*> lambdaArg;

    ExprPathItem(nix::Expr* e);
    ExprPathItem(nix::Expr* e, nix::Env* env);
    ExprPathItem(
        nix::Expr* e,
        nix::Env* env,
        std::optional<nix::Value*> lambdaArg
    );
};

struct Analysis {
    FileInfo fileInfo;
    std::string path;
    std::string basePath;
    std::string source;

    TraceableVector<ExprPathItem> exprPath;
    std::vector<Diagnostic> parseErrors;
    std::optional<ParseResultAttrPath> attr;
    std::optional<nix::Formal> formal;
    bool arg;
    bool uri;
    std::optional<ParseResultInherit> inherit;
};

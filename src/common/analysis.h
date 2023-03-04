#pragma once
#include <nix/nixexpr.hh>
#include "schema/schema.h"

struct NAParseError {
    std::string message;
    nix::Pos begin;
    nix::Pos end;
};

struct ParseResultAttrPath {
    size_t index;
    nix::AttrPath* attrPath;
};

struct ExprPathItem {
    nix::Expr* e;
    nix::Env* env;
    std::optional<nix::Value*> lambdaArg;

    ExprPathItem(nix::Expr* e);
};

struct Analysis {
    std::vector<ExprPathItem> exprPath;
    std::vector<NAParseError> parseErrors;
    std::string path;
    std::string basePath;
    std::optional<ParseResultAttrPath> attr;
    std::optional<nix::Formal> formal;
    // {} is no inherit. {{}} is inherit ...; {{expr}} is inherit (expr) ...;
    std::optional<std::optional<nix::Expr*>> inherit;
};
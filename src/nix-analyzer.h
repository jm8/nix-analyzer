#pragma once

#include <optional>
#include <variant>
#include "attr-set.hh"
#include "config.h"

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_allocator.h>
#include <gc/gc_cpp.h>
#endif

#include <iostream>
#include <memory>

#include "LibLsp/lsp/lsp_completion.h"
#include "eval-inline.hh"
#include "eval.hh"
#include "flake/flake.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "nixexpr.hh"
#include "shared.hh"
#include "store-api.hh"
#include "util.hh"

#include "logger.h"
#include "schema.h"

enum class FileType {
    None,
    Package,
    Flake,
    NixosModule,
};

struct FileInfo {
    nix::Path path;

    FileInfo();
    explicit FileInfo(nix::Path);
    FileInfo(nix::Path, FileType);

    FileType type;
    nix::Path nixpkgs();
};

nix::Path getDefaultNixpkgs();

template <typename T>
struct Spanned {
    T value;
    nix::Pos start;
    nix::Pos end;
};

struct Analysis {
    std::vector<nix::Expr*> exprPath;
    std::vector<nix::ParseError> parseErrors;
    std::string path;
    std::string basePath;
    std::vector<Spanned<nix::ExprPath*>> paths;
    std::optional<nix::CallbackAttrPath> attr;
    std::optional<nix::Formal> formal;
    // {} is no inherit. {{}} is inherit ...; {{expr}} is inherit (expr) ...;
    std::optional<std::optional<nix::Expr*>> inherit;
};

struct NAHoverResult {
    std::optional<std::string> text;
    std::optional<nix::Pos> pos;
};

struct NixAnalyzer
#if HAVE_BOEHMGC
    : gc
#endif
{
    std::unique_ptr<nix::EvalState> state;
    Logger& log;

    NixAnalyzer(const nix::Strings& searchPath,
                nix::ref<nix::Store> store,
                Logger& log);

    Analysis getExprPath(std::string source,
                         nix::Path path,
                         nix::Path basePath,
                         nix::Pos pos);

    std::pair<NACompletionType, std::vector<NACompletionItem>> complete(
        const Analysis& analysis,
        FileInfo file);

    NAHoverResult hover(const Analysis& analysis, FileInfo file);

    nix::Env* calculateEnv(std::vector<nix::Expr*> exprPath,
                           std::vector<std::optional<nix::Value*>>,
                           FileInfo file);

    // returns the env that sub would be evaluated in within super.
    // sub must be a direct child of super.
    nix::Env* updateEnv(nix::Expr* parent,
                        nix::Expr* child,
                        nix::Env* up,
                        std::optional<nix::Value*> lambdaArg);

    // returns a vector of the same length as exprPath.
    // if an element of exprPath is an ExprLambda, the corresponding
    // result is the calculated argument (or none if it can't figure it out).
    // otherwise it's none
    std::vector<std::optional<nix::Value*>> calculateLambdaArgs(
        std::vector<nix::Expr*> exprPath,
        FileInfo file);

    // returns the index into ExprPath of the expr containing the root of the
    // schema. then getSchema walks down to get the schema of the current
    // expression
    std::optional<std::pair<size_t, Schema>> getSchemaRoot(
        nix::Env& env,
        std::vector<nix::Expr*> exprPath,
        FileInfo file);

    std::optional<Schema> getSchema(nix::Env& env,
                                    std::vector<nix::Expr*> exprPath,
                                    FileInfo file);

    Schema nixosModuleSchema(nix::Path path);
};

int poscmp(nix::Pos a, nix::Pos b);
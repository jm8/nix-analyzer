#pragma once

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
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "nixexpr.hh"
#include "shared.hh"
#include "store-api.hh"
#include "util.hh"

#include "logger.h"

enum class FileType {
    None,
    Package,
};

struct FileInfo {
    nix::Path path;
    FileType type;
    nix::Path nixpkgs();
};

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
};

struct Schema;

struct SchemaItem {
    std::string name;
    std::string doc;
};

// a Schema represents the possible attributes a
// attrset can have
struct Schema {
    std::vector<SchemaItem> items;
};

#include "mkderivation-schema.h"

struct NACompletionItem {
    using Type = lsCompletionItemKind;

    std::string text;
    Type type;
    std::optional<std::string> documentation;
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

    std::vector<NACompletionItem> complete(std::vector<nix::Expr*> exprPath,
                                           FileInfo file);

    std::optional<nix::Pos> getPos(std::vector<nix::Expr*> exprPath,
                                   FileInfo file);

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

    // returns what attributes are expected to be on a direct child of parent
    std::optional<Schema> getSchema(nix::Env& env,
                                    nix::Expr* parent,
                                    nix::Expr* child);
};

int poscmp(nix::Pos a, nix::Pos b);
#pragma once
#include "na_config.h"
#include <nix/input-accessor.hh>
#include <nix/nixexpr.hh>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "eval.hh"
#include "fmt.hh"
#include "parser/tokenizer.h"
#include "position/position.h"
#include "value.hh"

struct Diagnostic {
    std::string msg;
    Range range;
};

struct ExprData {
    TokenRange range;
    std::shared_ptr<nix::StaticEnv> staticEnv;
    std::optional<nix::Expr*> parent;
    std::optional<nix::Env*> env;
    std::optional<nix::Value*> v;
};

// Represents an nix source file at a particular time with particular import
struct Document {
   private:
    nix::EvalState& state;
    nix::SourcePath path;
    std::string source;

    std::vector<Token> tokens;
    std::vector<Diagnostic> parseErrors;
    std::unordered_map<nix::Expr*, ExprData> exprData;
    std::optional<nix::Expr*> root;

   private:
    // initialize tokens, parseErrors, exprData, root
    void _parse();

   public:
    Document(nix::EvalState& state, nix::SourcePath path, std::string source);

    Range tokenRangeToRange(TokenRange tokenRange);

    nix::Expr* getRoot();
    std::shared_ptr<nix::StaticEnv> getStaticEnv(nix::Expr* e);
    nix::Env* getEnv(nix::Expr* e);
    std::optional<nix::Expr*> getParent(nix::Expr* e);
    nix::Value* thunk(nix::Expr* e, nix::Env* env);

   private:
    nix::Env* updateEnv(nix::Expr* parent, nix::Expr* child, nix::Env* up);
    void bindVars(
        std::shared_ptr<nix::StaticEnv> staticEnv,
        nix::Expr* e,
        std::optional<nix::Expr*> parent = {}
    );
};

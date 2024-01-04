#include <string>
#include <unordered_map>
#include <vector>
#include "input-accessor.hh"
#include "nixexpr.hh"
#include "parser/tokenizer.h"
#include "position/position.h"

struct ExprData {
    TokenRange range;
};

struct Diagnostic {
    std::string msg;
    Range range;
};

struct Document {
    nix::SourcePath path;
    std::vector<Token> tokens;
    std::vector<Diagnostic> parseErrors;
    std::unordered_map<nix::Expr*, ExprData> exprData;
    nix::Expr* root;
};

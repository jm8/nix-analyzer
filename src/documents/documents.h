#include <string>
#include <unordered_map>
#include <vector>
#include "input-accessor.hh"
#include "parser/tokenizer.h"
#include "position/position.h"

struct ExprData {
    TokenRange range;
};

struct Document {
    nix::SourcePath path;
    std::vector<Token> tokens;
    std::unordered_map<nix::Expr*, ExprData> exprData;
    nix::Expr* root;
};
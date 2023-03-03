#include <nix/nixexpr.hh>
#include <string>
#include <vector>

struct NAParseError {
    std::string message;
    nix::Pos begin;
    nix::Pos end;
};

struct ParseResultAttrPath {
    size_t index;
    nix::AttrPath* attrPath;
};

struct ParseResult {
    std::vector<nix::Expr*> exprPath;
    std::vector<NAParseError> parseErrors;
    std::string path;
    std::string basePath;
    std::optional<ParseResultAttrPath> attr;
    std::optional<nix::Formal> formal;
    // {} is no inherit. {{}} is inherit ...; {{expr}} is inherit (expr) ...;
    std::optional<std::optional<nix::Expr*>> inherit;
};

ParseResult parse(nix::EvalState& state,
                  std::string source,
                  nix::Path path,
                  nix::Path basePath,
                  nix::Pos targetPos);
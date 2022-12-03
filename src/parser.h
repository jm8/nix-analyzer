#include <string_view>

#include "nixexpr.hh"

struct ParseDiagnostic {
    nix::Pos pos;
    std::string error;
};

// Custom parser that
//   a) gives Pos to every Expr
//   b) is tolerant of syntax errors (lists them but keeps going)
// which is to be used in the currently open file
nix::Expr *parse(std::string_view s, std::vector<ParseDiagnostic> &diagnostics);
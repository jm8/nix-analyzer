#include "parser.h"
#include "test.h"
#include <glob.h>

using namespace std;
using namespace nix;

Expr *parse(string_view s, vector<ParseDiagnostic> &diagnostics) {
    return nullptr;
}

void runParseTests() {
    glob("*-okay-*.nix", 0, int (*errfunc)(const char *, int), glob_t *__restrict pglob)
}
#include "test.h"
#include "nix-analyzer.h"

using namespace std;
using namespace nix;

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());
    Expr *root = analyzer->parseString("import ./whatever.nix");
    analyzer->debugExpr(cout, root);
}
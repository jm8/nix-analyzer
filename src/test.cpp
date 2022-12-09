#include "test.h"
#include "debug.h"
#include "error.hh"
#include "nix-analyzer.h"

using namespace std;
using namespace nix;

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());
    const Pos targetPos{"test.nix", foFile, 2, 22};
    auto exprPath = analyzer->parsePathToFile("test.nix", targetPos);
    cout << "result:\n";
    for (auto e : exprPath) {
        cout << exprTypeName(e) << "\n";
    }
}
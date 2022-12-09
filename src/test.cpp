#include "test.h"
#include "error.hh"
#include "nix-analyzer.h"

using namespace std;
using namespace nix;

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());
    analyzer->state->parseExprFromFile("test.nix");
}
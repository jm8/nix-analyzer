#include "test.h"
#include <bits/ranges_algo.h>
#include <algorithm>
#include "debug.h"
#include "error.hh"
#include "nix-analyzer.h"
#include "nixexpr.hh"
#include "util.hh"
#include "value.hh"

using namespace std;
using namespace nix;

bool completionTest(NixAnalyzer& analyzer,
                    string beforeCursor,
                    string afterCursor,
                    vector<string>&& expected) {
    string source = beforeCursor + afterCursor;
    uint32_t line = 1;
    uint32_t col = 1;
    for (char c : beforeCursor) {
        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
    Pos pos{source, foString, line, col};
    auto exprPath = analyzer.parsePathToString(source, absPath("."), pos);
    auto completions = analyzer.complete(exprPath);
    sort(expected.begin(), expected.end());
    sort(completions.begin(), completions.end());
    bool good = expected == completions;
    if (good) {
        cout << "PASS"
             << "\n";
    } else {
        cout << "FAIL:\n" << source << "\n";
    }
    return good;
}

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore());

    completionTest(*analyzer, "{apple = 4; banana = 7; }.a", "",
                   {"apple", "banana"});
}
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

struct CompletionTest {
    string beforeCursor;
    string afterCursor;
    vector<string> expectedCompletions;
    vector<string> expectedErrors;

    bool run(NixAnalyzer& analyzer) {
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
        auto [exprPath, parseErrors] =
            analyzer.analyzeAtPos(source, absPath("."), pos);
        auto actualCompletions = analyzer.complete(exprPath);
        sort(expectedCompletions.begin(), expectedCompletions.end());
        sort(actualCompletions.begin(), actualCompletions.end());
        bool good = true;
        if (expectedCompletions != actualCompletions) {
            good = false;
            cout << "EXPECTED: ";
            for (auto s : expectedCompletions) {
                cout << s << " ";
            }
            cout << "\n";
            cout << "ACTUAL: ";
            for (auto s : actualCompletions) {
                cout << s << " ";
            }
            cout << "\n";
        }
        vector<string> actualErrors;
        for (ParseError error : parseErrors) {
            actualErrors.push_back(
                filterANSIEscapes(error.info().msg.str(), true));
        }
        if (actualErrors != expectedErrors) {
            good = false;
            cout << "EXPECTED: ";
            for (auto s : expectedErrors) {
                cout << s << " ";
            }
            cout << "\n";
            cout << "ACTUAL: ";
            for (auto s : actualErrors) {
                cout << s << " ";
            }
            cout << "\n";
        }
        if (good) {
            cout << "PASS\n";
        } else {
            cout << "FAIL:\n" << source << "\n";
        }
        return good;
    }
};

const vector<string> builtinIDs{
    "abort",      "baseNameOf",       "break",        "builtins",
    "derivation", "derivationStrict", "dirOf",        "false",
    "fetchGit",   "fetchMercurial",   "fetchTarball", "fetchTree",
    "fromTOML",   "import",           "isNull",       "map",
    "null",       "placeholder",      "removeAttrs",  "scopedImport",
    "throw",      "toString",         "true",
};

vector<string> builtinIDsPlus(const vector<string>& additional) {
    vector<string> result(builtinIDs);
    result.insert(result.end(), additional.begin(), additional.end());
    return result;
}

int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer =
        make_unique<NixAnalyzer>(searchPath, openStore("file:dummy"));

    vector<CompletionTest> completionTests{
        {
            "{apple = 4; banana = 7; }.a",
            "",
            {"apple", "banana"},
            {},
        },
        {
            "map",
            "",
            builtinIDs,
            {},
        },
        {
            "{a = 2; a = 3;}",
            "",
            {},
            {"attribute 'a' already defined at (string):1:2"},
        },
        {
            "{a, b, a}: a",
            "",
            builtinIDsPlus({"a", "b"}),
            {"duplicate formal function argument 'a'"},
        },
    };

    bool good = true;
    for (auto& test : completionTests) {
        if (!test.run(*analyzer))
            good = false;
    }

    return !good;
}
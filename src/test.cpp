#include <bits/ranges_algo.h>
#include <algorithm>
#include <iostream>
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
    string path;
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
        Path basePath = path.empty() ? absPath(".") : dirOf(path);
        auto [exprPath, parseErrors] =
            analyzer.analyzeAtPos(source, path, basePath, pos);
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
            cout << "PASS: " << source << "\n\n";
        } else {
            cout << "FAIL: " << source << "\n\n";
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

int main(int argc, char** argv) {
    initNix();
    initGC();

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [path to nixpkgs]\n";
        return 1;
    }

    PathView nixpkgs{argv[1]};

    Strings searchPath;
    auto analyzer =
        make_unique<NixAnalyzer>(searchPath, openStore("file:dummy"));

    vector<CompletionTest> completionTests{
        {
            .beforeCursor = "{apple = 4; banana = 7; }.a",
            .expectedCompletions = {"apple", "banana"},
        },
        {
            .beforeCursor = "map",
            .expectedCompletions = builtinIDs,
        },
        {
            .beforeCursor = "{a = 2; a = 3;}",
            .expectedCompletions = builtinIDs,
            .expectedErrors = {"attribute 'a' already defined at (string):1:2"},
        },
        {
            .beforeCursor = "{a, b, a}: a",
            .expectedCompletions = builtinIDsPlus({"a", "b"}),
            .expectedErrors = {"duplicate formal function argument 'a'"},
        },
        {
            .beforeCursor = "(abc)",
            .expectedCompletions = builtinIDs,
        },
        {
            .beforeCursor = "(2+)",
            .expectedCompletions = builtinIDs,
            .expectedErrors = {"syntax error, unexpected ')'"},
        },
        {
            .beforeCursor = "{abc = 2; def = \"green\";}.",
            .expectedCompletions = {"abc", "def"},
            .expectedErrors = {"syntax error, unexpected end of file, "
                               "expecting ID or OR_KW or "
                               "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor =
                "({ colors.red = 0; colors.green = 100; somethingelse = "
                "-1; }.colors.",
            .afterCursor = ")",
            .expectedCompletions = {"green", "red"},
            .expectedErrors =
                {"syntax error, unexpected ')', expecting ID or OR_KW or "
                 "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{ \"\" = { a = 1; }; }..",
            .expectedCompletions = {"a"},
            .expectedErrors =
                {"syntax error, unexpected '.', expecting ID or OR_KW or "
                 "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{a = 1, b = 2}",
            .expectedCompletions = builtinIDs,
            .expectedErrors = {"syntax error, unexpected ',', expecting ';'",
                               "syntax error, unexpected '}', expecting ';'"},
        },
        {
            .beforeCursor = "undefinedvariable.",
            .expectedCompletions = {},
            .expectedErrors = {"syntax error, unexpected end of file, "
                               "expecting ID or OR_KW or "
                               "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "(import \"" + nixpkgs + "\"{}).coqPackages.aaa",
            .expectedCompletions =
                {
                    "Cheerios",
                    "CoLoR",
                    "HoTT",
                    "ITree",
                    "InfSeqExt",
                    "LibHyps",
                    "QuickChick",
                    "StructTact",
                    "VST",
                    "Verdi",
                    "aac-tactics",
                    "addition-chains",
                    "autosubst",
                    "bignums",
                    "callPackage",
                    "category-theory",
                    "ceres",
                    "compcert",
                    "contribs",
                    "coq",
                    "coq-bits",
                    "coq-elpi",
                    "coq-ext-lib",
                    "coq-record-update",
                    "coqPackages",
                    "coqeal",
                    "coqide",
                    "coqprime",
                    "coquelicot",
                    "corn",
                    "deriving",
                    "dpdgraph",
                    "equations",
                    "extructures",
                    "filterPackages",
                    "flocq",
                    "fourcolor",
                    "gaia",
                    "gaia-hydras",
                    "gappalib",
                    "goedel",
                    "graph-theory",
                    "hierarchy-builder",
                    "hydra-battles",
                    "interval",
                    "iris",
                    "itauto",
                    "lib",
                    "math-classes",
                    "mathcomp",
                    "mathcomp-abel",
                    "mathcomp-algebra",
                    "mathcomp-algebra-tactics",
                    "mathcomp-analysis",
                    "mathcomp-bigenough",
                    "mathcomp-character",
                    "mathcomp-classical",
                    "mathcomp-field",
                    "mathcomp-fingroup",
                    "mathcomp-finmap",
                    "mathcomp-real-closed",
                    "mathcomp-solvable",
                    "mathcomp-ssreflect",
                    "mathcomp-tarjan",
                    "mathcomp-word",
                    "mathcomp-zify",
                    "metaFetch",
                    "metacoq",
                    "metacoq-erasure",
                    "metacoq-pcuic",
                    "metacoq-safechecker",
                    "metacoq-template-coq",
                    "metalib",
                    "mkCoqDerivation",
                    "multinomials",
                    "newScope",
                    "odd-order",
                    "overrideScope",
                    "overrideScope'",
                    "packages",
                    "paco",
                    "paramcoq",
                    "parsec",
                    "pocklington",
                    "recurseForDerivations",
                    "reglang",
                    "relation-algebra",
                    "semantics",
                    "serapi",
                    "simple-io",
                    "ssreflect",
                    "stdpp",
                    "tlc",
                    "topology",
                    "trakt",
                    "zorns-lemma",
                },
        },
        {
            .beforeCursor = "with {a = 2; b = 3;}; ",
            .expectedCompletions = builtinIDs,
            .expectedErrors = {"syntax error, unexpected end of file"},
        },
    };

    bool good = true;
    for (auto& test : completionTests) {
        if (!test.run(*analyzer))
            good = false;
    }

    return !good;
}
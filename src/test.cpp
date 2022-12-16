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
int main() {
    initNix();
    initGC();

    Strings searchPath;
    auto analyzer =
        make_unique<NixAnalyzer>(searchPath, openStore("file:dummy"));

    vector<CompletionTest> completionTests{
        {"{apple = 4; banana = 7; }.a", "", {"apple", "failthetest"}, {}},
        // {"(import <nixpkgs> {}).coqPackages.whatev",
        //  "",
        //  {"Cheerios",
        //   "dpdgraph",
        //   "mathcomp-tarjan",
        //   "CoLoR",
        //   "equations",
        //   "mathcomp-word",
        //   "ITree",
        //   "extructures",
        //   "mathcomp-zify",
        //   "InfSeqExt",
        //   "filterPackages",
        //   "metalib",
        //   "QuickChick",
        //   "flocq",
        //   "mkCoqDerivation",
        //   "StructTact",
        //   "fourcolor",
        //   "multinomials",
        //   "VST",
        //   "gaia",
        //   "newScope",
        //   "Verdi",
        //   "gaia-hydras",
        //   "odd-order",
        //   "aac-tactics",
        //   "gappalib",
        //   "overrideScope",
        //   "addition-chains",
        //   "goedel",
        //   "overrideScope'",
        //   "autosubst",
        //   "graph-theory",
        //   "packages",
        //   "bignums",
        //   "hierarchy-builder",
        //   "paco",
        //   "callPackage",
        //   "hydra-battles",
        //   "paramcoq",
        //   "category-theory",
        //   "interval",
        //   "parsec",
        //   "ceres",
        //   "iris",
        //   "pocklington",
        //   "compcert",
        //   "itauto",
        //   "recurseForDerivations",
        //   "contribs",
        //   "lib",
        //   "reglang",
        //   "coq",
        //   "math-classes",
        //   "relation-algebra",
        //   "coq-bits",
        //   "mathcomp",
        //   "semantics",
        //   "coq-elpi",
        //   "mathcomp-abel",
        //   "serapi",
        //   "coq-ext-lib",
        //   "mathcomp-algebra",
        //   "simple-io",
        //   "coq-record-update",
        //   "mathcomp-analysis",
        //   "smpl",
        //   "coqPackages",
        //   "mathcomp-bigenough",
        //   "ssreflect",
        //   "coqeal",
        //   "mathcomp-character",
        //   "stdpp",
        //   "coqhammer",
        //   "mathcomp-field",
        //   "tlc",
        //   "coqprime",
        //   "mathcomp-fingroup",
        //   "topology",
        //   "coqtail-math",
        //   "mathcomp-finmap",
        //   "zorns-lemma",
        //   "coquelicot",
        //   "mathcomp-real-closed",
        //   "corn",
        //   "mathcomp-solvable",
        //   "deriving",
        //   "mathcomp-ssreflect"},
        //  {}},
        {"map",
         "",
         {
             "__add",
             "__addErrorContext",
             "__all",
             "__any",
             "__appendContext",
             "__attrNames",
             "__attrValues",
             "__bitAnd",
             "__bitOr",
             "__bitXor",
             "__catAttrs",
             "__ceil",
             "__compareVersions",
             "__concatLists",
             "__concatMap",
             "__concatStringsSep",
             "__currentSystem",
             "__currentTime",
             "__deepSeq",
             "__div",
             "__elem",
             "__elemAt",
             "__fetchurl",
             "__filter",
             "__filterSource",
             "__findFile",
             "__floor",
             "__foldl'",
             "__fromJSON",
             "__functionArgs",
             "__genList",
             "__genericClosure",
             "__getAttr",
             "__getContext",
             "__getEnv",
             //  "__getFlake",
             "__groupBy",
             "__hasAttr",
             "__hasContext",
             "__hashFile",
             "__hashString",
             "__head",
             "__intersectAttrs",
             "__isAttrs",
             "__isBool",
             "__isFloat",
             "__isFunction",
             "__isInt",
             "__isList",
             "__isPath",
             "__isString",
             "__langVersion",
             "__length",
             "__lessThan",
             "__listToAttrs",
             "__mapAttrs",
             "__match",
             "__mul",
             "__nixPath",
             "__nixVersion",
             "__parseDrvName",
             "__partition",
             "__path",
             "__pathExists",
             "__readDir",
             "__readFile",
             "__replaceStrings",
             "__seq",
             "__sort",
             "__split",
             "__splitVersion",
             "__storeDir",
             "__storePath",
             "__stringLength",
             "__sub",
             "__substring",
             "__tail",
             "__toFile",
             "__toJSON",
             "__toPath",
             "__toXML",
             "__trace",
             "__traceVerbose",
             "__tryEval",
             "__typeOf",
             "__unsafeDiscardOutputDependency",
             "__unsafeDiscardStringContext",
             "__unsafeGetAttrPos",
             "__zipAttrsWith",
             "abort",
             "baseNameOf",
             "break",
             "builtins",
             "derivation",
             "derivationStrict",
             "dirOf",
             "false",
             "fetchGit",
             "fetchMercurial",
             "fetchTarball",
             "fetchTree",
             "fromTOML",
             "import",
             "isNull",
             "map",
             "null",
             "placeholder",
             "removeAttrs",
             "scopedImport",
             "throw",
             "toString",
             "true",
         },
         {}},
        {"{a = 2; a = 3;}",
         "",
         {},
         {"attribute 'a' already defined at (string):1:2"}},
    };

    bool good = true;
    for (auto& test : completionTests) {
        if (!test.run(*analyzer))
            good = false;
    }

    return !good;
}
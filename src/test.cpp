#include <bits/ranges_algo.h>
#include <algorithm>
#include <iostream>
#include <optional>
#include "debug.h"
#include "error.hh"
#include "logger.h"
#include "nix-analyzer.h"
#include "nixexpr.hh"
#include "util.hh"
#include "value.hh"

using namespace std;
using namespace std::literals;
using namespace nix;

void printStrings(const vector<string>& strings) {
    const char* sep = "";
    cout << "{";
    for (auto& string : strings) {
        cout << sep << '"' << string << '"';
        sep = ", ";
    }
    cout << "}";
}

struct CompletionTest {
    string beforeCursor;
    string afterCursor;
    // position overrides before cursor and after cursor
    optional<pair<uint32_t, uint32_t>> position;
    string path;
    FileType ftype;
    vector<string> expectedCompletions;
    vector<string> expectedErrors;

    bool run(NixAnalyzer& analyzer) {
        string source = beforeCursor + afterCursor;
        Pos pos;
        if (position) {
            pos = {path, foFile, position->first, position->second};
        } else {
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
            pos = {source, foString, line, col};
        }
        cout << pos.file << "\n";
        Path basePath = path.empty() ? absPath(".") : dirOf(path);
        auto analysis = analyzer.getExprPath(source, path, basePath, pos);
        vector<string> actualCompletions;
        auto [completionType, completionItems] =
            analyzer.complete(analysis, FileInfo{path});
        for (auto completionItem : completionItems) {
            actualCompletions.push_back(completionItem.text);
        }
        sort(expectedCompletions.begin(), expectedCompletions.end());
        sort(actualCompletions.begin(), actualCompletions.end());
        bool good = true;
        if (expectedCompletions != actualCompletions) {
            good = false;
            cout << "EXPECTED: ";
            printStrings(expectedCompletions);
            cout << "\n";
            cout << "ACTUAL: ";
            printStrings(actualCompletions);
            cout << "\n";
        }
        vector<string> actualErrors;
        for (ParseError error : analysis.parseErrors) {
            actualErrors.push_back(
                filterANSIEscapes(error.info().msg.str(), true));
        }
        if (actualErrors != expectedErrors) {
            good = false;
            cout << "EXPECTED: ";
            printStrings(expectedErrors);
            cout << "\n";
            cout << "ACTUAL: ";
            printStrings(actualErrors);
            cout << "\n";
        }
        if (good) {
            cout << "PASS"
                 << "\n\n";
        } else {
            cout << "FAIL"
                 << "\n\n";
        }
        return good;
    }
};

struct GetPosTest {
    string beforeCursor;
    string afterCursor;
    // position overrides before cursor and after cursor
    optional<pair<uint32_t, uint32_t>> position;
    string path;
    FileType ftype;
    optional<Pos> expectedPos;

    bool run(NixAnalyzer& analyzer) {
        string source = beforeCursor + afterCursor;
        Pos pos;
        if (position) {
            pos = {path, foFile, position->first, position->second};
        } else {
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
            pos = {source, foString, line, col};
        }
        cout << pos.file << "\n";
        Path basePath = path.empty() ? absPath(".") : dirOf(path);
        auto analysis = analyzer.getExprPath(source, path, basePath, pos);
        auto hoverResult = analyzer.hover(analysis, {path, ftype});
        auto actualPos = hoverResult.pos;

        bool good = true;
        if (actualPos.has_value() != expectedPos.has_value()) {
            good = false;
        }
        if (expectedPos && actualPos) {
            if (expectedPos->line != actualPos->line)
                good = false;
            if (expectedPos->column != actualPos->column)
                good = false;
            if (expectedPos->origin != actualPos->origin)
                good = false;
            if (expectedPos->origin == foFile &&
                (expectedPos->file != actualPos->file))
                good = false;
        }
        if (good) {
            cout << "PASS"
                 << "\n\n";
        } else {
            if (expectedPos) {
                cout << "EXPECTED: " << *expectedPos << "\n";
            } else {
                cout << "EXPECTED: none\n";
            }
            if (actualPos) {
                cout << "ACTUAL: " << *actualPos << "\n";
            } else {
                cout << "ACTUAL: none\n";
            }
            cout << "FAIL"
                 << "\n\n";
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

enum class TestOption {
    All,
    LastCompletion,
    LastGetPos,
};

int main(int argc, char** argv) {
    initNix();
    initGC();

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " [path to nixpkgs] [lastcompletion|lastgetpos]?\n";
        return 1;
    }

    auto testOption = TestOption::All;
    if (argc == 3) {
        if (string_view(argv[2]) == "lastcompletion") {
            testOption = TestOption::LastCompletion;
        } else if (string_view(argv[2]) == "lastgetpos") {
            testOption = TestOption::LastGetPos;
        } else {
            std::cerr << "Usage: " << argv[0]
                      << " [path to nixpkgs] [lastcompletion|lastgetpos]?\n";
            return 1;
        }
    }

    Path nixpkgs{argv[1]};

    Strings searchPath;
    ::Logger log{"log.txt"};
    auto analyzer = make_unique<NixAnalyzer>(searchPath, openStore(), log);

    Path allpackages{nixpkgs + "/pkgs/top-level/all-packages.nix"s};
    string allpackagesContent{readFile(allpackages)};
    Path graphviz{nixpkgs + "/pkgs/tools/graphics/graphviz/default.nix"s};

    vector<string> coqPackages{
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
    };

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
            .expectedCompletions = {},
            .expectedErrors = {"attribute 'a' already "
                               "defined at (string):1:2"},
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
            .beforeCursor = "({ colors.red = 0; colors.green = 100; "
                            "somethingelse = "
                            "-1; }.colors.",
            .afterCursor = ")",
            .expectedCompletions = {"green", "red"},
            .expectedErrors = {"syntax error, unexpected ')', expecting "
                               "ID or OR_KW or "
                               "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{ \"\" = { a = 1; }; }..",
            .expectedCompletions = {"a"},
            .expectedErrors = {"syntax error, unexpected '.', expecting "
                               "ID or OR_KW or "
                               "DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{a = 1, b = 2}",
            .expectedCompletions = {},
            .expectedErrors = {"syntax error, unexpected "
                               "',', expecting ';'",
                               "syntax error, unexpected "
                               "'}', expecting ';'"},
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
            .expectedCompletions = coqPackages,
        },
        {
            .beforeCursor = "with {a = 2; b = 3;}; ",
            .expectedCompletions = builtinIDsPlus({"a", "b"}),
            .expectedErrors = {"syntax error, unexpected end of file"},
        },
        {
            .beforeCursor = allpackagesContent,
            .position = {{113, 28}},
            .path = allpackages,
            .expectedCompletions =
                builtinIDsPlus({"lib", "super", "overlays", "pkgs", "noSysDirs",
                                "res", "config"}),
        },
        {
            .beforeCursor = readFile(graphviz),
            .position = {{68, 15}},
            .path = graphviz,
            .ftype = FileType::Package,
            .expectedCompletions =
                {
                    "__unfix__",
                    "add",
                    "addContextFrom",
                    "addErrorContext",
                    "addErrorContextToAttrs",
                    "addMetaAttrs",
                    "all",
                    "and",
                    "any",
                    "appendToName",
                    "applyModuleArgsIfFunction",
                    "assertMsg",
                    "assertOneOf",
                    "asserts",
                    "attrByPath",
                    "attrNames",
                    "attrNamesToStr",
                    "attrVals",
                    "attrValues",
                    "attrsets",
                    "bitAnd",
                    "bitNot",
                    "bitOr",
                    "bitXor",
                    "boolToString",
                    "callPackageWith",
                    "callPackagesWith",
                    "canCleanSource",
                    "cartesianProductOfSets",
                    "catAttrs",
                    "checkFlag",
                    "checkListOfEnum",
                    "checkReqs",
                    "chooseDevOutputs",
                    "cleanSource",
                    "cleanSourceFilter",
                    "cleanSourceWith",
                    "cli",
                    "closePropagation",
                    "collect",
                    "commitIdFromGitRepo",
                    "compare",
                    "compareLists",
                    "composeExtensions",
                    "composeManyExtensions",
                    "concat",
                    "concatImapStrings",
                    "concatImapStringsSep",
                    "concatLists",
                    "concatMap",
                    "concatMapAttrs",
                    "concatMapStrings",
                    "concatMapStringsSep",
                    "concatStrings",
                    "concatStringsSep",
                    "condConcat",
                    "const",
                    "converge",
                    "count",
                    "crossLists",
                    "customisation",
                    "debug",
                    "deepSeq",
                    "defaultFunctor",
                    "defaultMerge",
                    "defaultMergeArg",
                    "defaultTypeMerge",
                    "derivations",
                    "dischargeProperties",
                    "doRename",
                    "dontDistribute",
                    "dontRecurseIntoAttrs",
                    "drop",
                    "elem",
                    "elemAt",
                    "enableFeature",
                    "enableFeatureAs",
                    "escape",
                    "escapeRegex",
                    "escapeShellArg",
                    "escapeShellArgs",
                    "escapeXML",
                    "evalModules",
                    "evalOptionValue",
                    "extend",
                    "extendDerivation",
                    "extends",
                    "fakeHash",
                    "fakeSha256",
                    "fakeSha512",
                    "fetchers",
                    "fileContents",
                    "filesystem",
                    "filter",
                    "filterAttrs",
                    "filterAttrsRecursive",
                    "filterOverrides",
                    "findFirst",
                    "findSingle",
                    "fix",
                    "fix'",
                    "fixMergeModules",
                    "fixedPoints",
                    "fixedWidthNumber",
                    "fixedWidthString",
                    "fixupOptionType",
                    "flatten",
                    "flip",
                    "fold",
                    "foldArgs",
                    "foldAttrs",
                    "foldl",
                    "foldl'",
                    "foldr",
                    "forEach",
                    "fullDepEntry",
                    "functionArgs",
                    "genAttrs",
                    "genList",
                    "generators",
                    "genericClosure",
                    "getAttr",
                    "getAttrFromPath",
                    "getAttrs",
                    "getBin",
                    "getDev",
                    "getExe",
                    "getFiles",
                    "getLib",
                    "getLicenseFromSpdxId",
                    "getMan",
                    "getName",
                    "getOutput",
                    "getValue",
                    "getValues",
                    "getVersion",
                    "groupBy",
                    "groupBy'",
                    "hasAttr",
                    "hasAttrByPath",
                    "hasInfix",
                    "hasPrefix",
                    "hasSuffix",
                    "head",
                    "hiPrio",
                    "hiPrioSet",
                    "hydraJob",
                    "id",
                    "ifEnable",
                    "imap",
                    "imap0",
                    "imap1",
                    "importJSON",
                    "importTOML",
                    "inNixShell",
                    "inPureEvalMode",
                    "info",
                    "init",
                    "innerClosePropagation",
                    "innerModifySumArgs",
                    "intersectLists",
                    "intersperse",
                    "isAttrs",
                    "isBool",
                    "isDerivation",
                    "isFloat",
                    "isFunction",
                    "isInOldestRelease",
                    "isInt",
                    "isList",
                    "isOption",
                    "isOptionType",
                    "isStorePath",
                    "isString",
                    "isType",
                    "isValidPosixName",
                    "kernel",
                    "last",
                    "lazyDerivation",
                    "lazyGenericClosure",
                    "length",
                    "lessThan",
                    "licenses",
                    "listDfs",
                    "listToAttrs",
                    "lists",
                    "literalDocBook",
                    "literalExample",
                    "literalExpression",
                    "literalMD",
                    "lowPrio",
                    "lowPrioSet",
                    "lowerChars",
                    "maintainers",
                    "makeBinPath",
                    "makeExtensible",
                    "makeExtensibleWithCustomName",
                    "makeLibraryPath",
                    "makeOverridable",
                    "makeScope",
                    "makeScopeWithSplicing",
                    "makeSearchPath",
                    "makeSearchPathOutput",
                    "mapAttrs",
                    "mapAttrs'",
                    "mapAttrsFlatten",
                    "mapAttrsRecursive",
                    "mapAttrsRecursiveCond",
                    "mapAttrsToList",
                    "mapDerivationAttrset",
                    "mapNullable",
                    "matchAttrs",
                    "max",
                    "maybeAttr",
                    "maybeAttrNullable",
                    "maybeEnv",
                    "mdDoc",
                    "mergeAttrBy",
                    "mergeAttrByFunc",
                    "mergeAttrs",
                    "mergeAttrsByFuncDefaults",
                    "mergeAttrsByFuncDefaultsClean",
                    "mergeAttrsConcatenateValues",
                    "mergeAttrsNoOverride",
                    "mergeAttrsWithFunc",
                    "mergeDefaultOption",
                    "mergeDefinitions",
                    "mergeEqualOption",
                    "mergeModules",
                    "mergeModules'",
                    "mergeOneOption",
                    "mergeOptionDecls",
                    "mergeUniqueOption",
                    "meta",
                    "min",
                    "misc",
                    "mkAfter",
                    "mkAliasAndWrapDefinitions",
                    "mkAliasDefinitions",
                    "mkAliasOptionModule",
                    "mkAssert",
                    "mkBefore",
                    "mkChangedOptionModule",
                    "mkDefault",
                    "mkDerivedConfig",
                    "mkEnableOption",
                    "mkFixStrictness",
                    "mkForce",
                    "mkIf",
                    "mkImageMediaOverride",
                    "mkMerge",
                    "mkMergedOptionModule",
                    "mkOption",
                    "mkOptionDefault",
                    "mkOptionType",
                    "mkOrder",
                    "mkOverride",
                    "mkPackageOption",
                    "mkRemovedOptionModule",
                    "mkRenamedOptionModule",
                    "mkRenamedOptionModuleWith",
                    "mkSinkUndeclaredOptions",
                    "mkVMOverride",
                    "mod",
                    "modifySumArgs",
                    "modules",
                    "mutuallyExclusive",
                    "nameFromURL",
                    "nameValuePair",
                    "naturalSort",
                    "nixType",
                    "nixpkgsVersion",
                    "noDepEntry",
                    "nvs",
                    "optionAttrSetToDocList",
                    "optionAttrSetToDocList'",
                    "optional",
                    "optionalAttrs",
                    "optionalString",
                    "optionals",
                    "options",
                    "or",
                    "overrideDerivation",
                    "overrideExisting",
                    "packEntry",
                    "partition",
                    "pathExists",
                    "pathHasContext",
                    "pathIsDirectory",
                    "pathIsGitRepo",
                    "pathIsRegularFile",
                    "pathType",
                    "pipe",
                    "platforms",
                    "pushDownProperties",
                    "range",
                    "readFile",
                    "readPathsFromFile",
                    "recurseIntoAttrs",
                    "recursiveUpdate",
                    "recursiveUpdateUntil",
                    "remove",
                    "removePrefix",
                    "removeSuffix",
                    "replaceChars",
                    "replaceStrings",
                    "reverseList",
                    "runTests",
                    "scrubOptionValue",
                    "seq",
                    "setAttr",
                    "setAttrByPath",
                    "setAttrMerge",
                    "setDefaultModuleLocation",
                    "setFunctionArgs",
                    "setName",
                    "setPrio",
                    "setType",
                    "showAttrPath",
                    "showFiles",
                    "showOption",
                    "showOptionWithDefLocs",
                    "showVal",
                    "showWarnings",
                    "singleton",
                    "sort",
                    "sortProperties",
                    "sourceByRegex",
                    "sourceFilesBySuffices",
                    "sourceTypes",
                    "sources",
                    "splitByAndCompare",
                    "splitString",
                    "splitVersion",
                    "stringAfter",
                    "stringAsChars",
                    "stringLength",
                    "stringToCharacters",
                    "strings",
                    "stringsWithDeps",
                    "sub",
                    "sublist",
                    "substring",
                    "subtractLists",
                    "systems",
                    "tail",
                    "take",
                    "teams",
                    "testAllTrue",
                    "textClosureList",
                    "textClosureMap",
                    "throwIf",
                    "throwIfNot",
                    "toBaseDigits",
                    "toDerivation",
                    "toFunction",
                    "toHexString",
                    "toInt",
                    "toIntBase10",
                    "toList",
                    "toLower",
                    "toShellVar",
                    "toShellVars",
                    "toUpper",
                    "toposort",
                    "trace",
                    "traceCall",
                    "traceCall2",
                    "traceCall3",
                    "traceCallXml",
                    "traceFnSeqN",
                    "traceIf",
                    "traceSeq",
                    "traceSeqN",
                    "traceShowVal",
                    "traceShowValMarked",
                    "traceVal",
                    "traceValFn",
                    "traceValIfNot",
                    "traceValSeq",
                    "traceValSeqFn",
                    "traceValSeqN",
                    "traceValSeqNFn",
                    "traceXMLVal",
                    "traceXMLValMarked",
                    "trivial",
                    "types",
                    "unifyModuleSyntax",
                    "uniqList",
                    "uniqListExt",
                    "unique",
                    "unknownModule",
                    "updateManyAttrsByPath",
                    "updateName",
                    "upperChars",
                    "version",
                    "versionAtLeast",
                    "versionOlder",
                    "versions",
                    "warn",
                    "warnIf",
                    "warnIfNot",
                    "withFeature",
                    "withFeatureAs",
                    "zip",
                    "zipAttrs",
                    "zipAttrsWith",
                    "zipAttrsWithNames",
                    "zipLists",
                    "zipListsWith",
                    "zipWithNames",
                },
        },
        {
            .beforeCursor = "{coqPackages}: coqPackages.",
            .ftype = FileType::Package,
            .expectedCompletions = coqPackages,
            .expectedErrors =
                {"syntax error, unexpected end of file, expecting "
                 "ID or OR_KW or DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{stdenv}: stdenv.mkDerivation {",
            .afterCursor = "}",
            .ftype = FileType::Package,
            .expectedCompletions =
                {
                    "NIX_DEBUG",
                    "buildFlags",
                    "buildFlagsArray",
                    "buildInputs",
                    "buildPhase",
                    "checkFlags",
                    "checkFlagsArray",
                    "checkInputs",
                    "checkPhase",
                    "checkTarget",
                    "configureFlags",
                    "configureFlagsArray",
                    "configurePhase",
                    "configurePlatforms",
                    "configureScript",
                    "depsBuildBuild",
                    "depsBuildBuildPropagated",
                    "depsBuildTarget",
                    "depsBuildTargetPropagated",
                    "depsHostHost",
                    "depsHostHostPropagated",
                    "depsTargetTarget",
                    "depsTargetTargetPropagated",
                    "distFlags",
                    "distFlagsArray",
                    "distPhase",
                    "distTarget",
                    "doCheck",
                    "doInstallCheck",
                    "dontAddDisableDepTrack",
                    "dontAddPrefix",
                    "dontBuild",
                    "dontConfigure",
                    "dontCopyDist",
                    "dontDisableStatic",
                    "dontFixLibtool",
                    "dontFixup",
                    "dontInstall",
                    "dontMakeSourcesWritable",
                    "dontMoveSbin",
                    "dontPatch",
                    "dontPatchELF",
                    "dontPatchShebangs",
                    "dontPruneLibtoolFiles",
                    "dontStrip",
                    "dontStripHost",
                    "dontStripTarget",
                    "dontUnpack",
                    "enableParallelBuilding",
                    "fixupPhase",
                    "forceShare",
                    "installCheckFlags",
                    "installCheckFlagsArray",
                    "installCheckInputs",
                    "installCheckPhase",
                    "installCheckTarget",
                    "installFlags",
                    "installFlagsArray",
                    "installPhase",
                    "installTargets",
                    "makeFlags",
                    "makeFlagsArray",
                    "makefile",
                    "nativeBuildInputs",
                    "passthru",
                    "patchFlags",
                    "patchPhase",
                    "patches",
                    "phases",
                    "postBuild",
                    "postCheck",
                    "postConfigure",
                    "postDist",
                    "postFixup",
                    "postInstall",
                    "postInstallCheck",
                    "postPatch",
                    "postPhases",
                    "postUnpack",
                    "preBuild",
                    "preBuildPhases",
                    "preCheck",
                    "preConfigure",
                    "preConfigurePhases",
                    "preDist",
                    "preDistPhases",
                    "preFixup",
                    "preFixupPhases",
                    "preInstall",
                    "preInstallCheck",
                    "preInstallPhases",
                    "prePatch",
                    "prePhases",
                    "preUnpack",
                    "prefix",
                    "prefixKey",
                    "propagatedBuildInputs",
                    "propagatedNativeBuildInputs",
                    "setSourceRoot",
                    "setupHook",
                    "sourceRoot",
                    "src",
                    "srcs",
                    "stripAllFlags",
                    "stripAllList",
                    "stripAllListTarget",
                    "stripDebugFlags",
                    "stripDebugList",
                    "stripDebugListTarget",
                    "tarballs",
                    "unpackCmd",
                    "unpackPhase",
                },
        },
        {
            .beforeCursor = "rec { a = [ ",
            .afterCursor = " ]; b = 2; }",
            .expectedCompletions = builtinIDsPlus({"a", "b"}),
        },
        {
            .beforeCursor = "let a = { b = 3; }; in rec { s = [ a.aaa",
            .afterCursor = " ]; }",
            .expectedCompletions = {"b"},
        },
        {
            .beforeCursor = "let a = { b = 3; }; in { s = [ a.aaa",
            .afterCursor = " ]; }",
            .expectedCompletions = {"b"},
        },
        {
            .beforeCursor =
                "let x = { aaa = { bbb = 2; }; }; in { inherit (x.aaa.",
            .afterCursor = ") whatever; }",
            .expectedCompletions = {"bbb"},
            .expectedErrors = {"syntax error, unexpected ')', expecting ID or "
                               "OR_KW or DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "{ inherit (a.",
            .afterCursor = "); }",
            .expectedErrors = {"syntax error, unexpected ')', expecting ID or "
                               "OR_KW or DOLLAR_CURLY or '\"'"},
        },
        {
            .beforeCursor = "with { a = {b = 1; }; }; a.que",
            .expectedCompletions = {"b"},
        },
        {
            .beforeCursor = "with null; x",
            .expectedCompletions = builtinIDs,
        },
        {
            .beforeCursor = "let a = 1; in { A = with ",
            .afterCursor = "; B = 2 }",
            .expectedCompletions = builtinIDsPlus({"a"}),
            .expectedErrors = {"syntax error, unexpected ';'"},
        },
        {
            .beforeCursor = "({a, b}: a) { ",
            .afterCursor = "}",
            .expectedCompletions = {"a", "b"},
        },
        {
            .beforeCursor = readFile("./flake.nix"),
            .position = {{21, 39}},
            .path = absPath("./flake.nix"),
            .ftype = FileType::Flake,
            .expectedCompletions = {"aarch64-darwin", "aarch64-linux",
                                    "armv5tel-linux", "armv6l-linux",
                                    "armv7l-linux", "i686-linux",
                                    "mipsel-linux", "powerpc64le-linux",
                                    "riscv64-linux", "x86_64-darwin",
                                    "x86_64-linux"},
        },
        {
            .beforeCursor = " let a = undefinedvariable; in aaa",
            .expectedCompletions = builtinIDsPlus({"a"}),
        },
        {
            .beforeCursor =
                R"(let a = {b = {c = 2; d = { e = 2; }; }; }; in a.${"" + "b"}.)",
            .afterCursor = "d.e",
            .expectedCompletions = {"c", "d"},
        },
        {
            .beforeCursor = "{graphviz}: graphviz.override { ",
            .afterCursor = " }",
            .ftype = FileType::Package,
            .expectedCompletions =
                {
                    "ApplicationServices",
                    "autoreconfHook",
                    "bash",
                    "bison",
                    "cairo",
                    "exiv2",
                    "expat",
                    "fetchFromGitLab",
                    "fetchpatch",
                    "flex",
                    "fltk",
                    "fontconfig",
                    "gd",
                    "gts",
                    "lib",
                    "libdevil",
                    "libjpeg",
                    "libpng",
                    "libtool",
                    "pango",
                    "pkg-config",
                    "python3",
                    "stdenv",
                    "withXorg",
                    "xorg",
                },
        },
        {
            .beforeCursor = "let aaa = 2; in { inherit a",
            .afterCursor = "; }",
            .expectedCompletions = builtinIDsPlus({"aaa"}),
        },
        {
            .beforeCursor = "let a = { b = 3; }; in { inherit (a) x",
            .afterCursor = "; }",
            .expectedCompletions = {"b"},
        },
        // I don't know why this doesn't work
        // {
        //     .beforeCursor = "let a = { b = 3; }; in { inherit (a) ",
        //     .afterCursor = "; }",
        //     .expectedCompletions = {"b"},
        // },
        // parser changes required
        // {
        //     .beforeCursor = "let a = {b = 2;}; c = a.",
        //     .afterCursor = " in a",
        //     .expectedCompletions = {"b"},
        // },
    };

    vector<GetPosTest> getPosTests{
        {
            .beforeCursor = "{pkgs}: with pkgs; graph",
            .afterCursor = "viz",
            .ftype = FileType::Package,
            .expectedPos = Pos{nixpkgs + "/pkgs/top-level/all-packages.nix",
                               foFile, 7643, 3},
        },
        {
            .beforeCursor = "let apple = { banana = 2; }; in apple.ba",
            .afterCursor = "nana",
            .expectedPos = Pos{"", foString, 1, 15},
        },
        {
            .beforeCursor = "{pkgs}: pkgs.graph",
            .afterCursor = "viz",
            .ftype = FileType::Package,
            .expectedPos = Pos{nixpkgs + "/pkgs/top-level/all-packages.nix",
                               foFile, 7643, 3},
        },
        {
            .beforeCursor = "let a = { b = { c = 2; }; }; in a.b",
            .afterCursor = ".c",
            .expectedPos = Pos{"", foString, 1, 11},
        },
        {
            .beforeCursor = "{graph",
            .afterCursor = "viz}:",
            .ftype = FileType::Package,
            .expectedPos = Pos{nixpkgs + "/pkgs/top-level/all-packages.nix",
                               foFile, 7643, 3},
        },
        {
            .beforeCursor = "{graphviz}: { inherit graph",
            .afterCursor = "viz; }",
            .ftype = FileType::Package,
            .expectedPos = Pos{nixpkgs + "/pkgs/top-level/all-packages.nix",
                               foFile, 7643, 3},
        }};
    bool good = true;
    if (testOption == TestOption::All) {
        for (auto& test : completionTests) {
            if (!test.run(*analyzer))
                good = false;
        }
        for (auto& test : getPosTests) {
            if (!test.run(*analyzer))
                good = false;
        }
    } else if (testOption == TestOption::LastCompletion) {
        if (!completionTests.back().run(*analyzer))
            good = false;
    } else if (testOption == TestOption::LastGetPos) {
        if (!getPosTests.back().run(*analyzer))
            good = false;
    }

    if (!good) {
        cout << "A test failed\n";
        return 1;
    }

    cout << "All good\n";
}
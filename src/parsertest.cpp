#include "na_config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <pstl/glue_execution_defs.h>
#include <string>
#include <vector>
#include "canon-path.hh"
#include "error.hh"
#include "input-accessor.hh"
#include "parser/parser.h"
#include "search-path.hh"
#include "stringify/stringify.h"

int currentProgress = 26996;

bool hasArgv = false;

bool check_consistency(nix::EvalState& state, std::string path) {
    std::cout << path << " ";
    std::cout.flush();

    std::string source = nix::readFile(path);

    if (source.size() > 100000) {
        std::cout << "skipping\n";
        return false;
    }
    auto basePath = nix::absPath(nix::dirOf(path));

    auto document = parse(
        state,
        nix::SourcePath{state.rootFS, nix::CanonPath{path}},
        nix::SourcePath{state.rootFS, nix::CanonPath{basePath}},
        source
    );
    nix::Expr* actual = document.root;

    nix::Expr* expected;
    try {
        expected = state.parseExprFromString(
            source, nix::SourcePath{state.rootFS, nix::CanonPath{basePath}}
        );
    } catch (nix::Error& e) {
        std::cout << "ITSSUPPOSEDTOERR\n";
        return true;
    }

    auto actualS = stringify(state, actual);
    auto expectedS = stringify(state, expected);

    if (hasArgv) {
        std::cerr << "ACTUAL\n" << actualS << "\n";
        for (auto err : document.parseErrors) {
            std::cerr << err.msg << " " << err.range << "\n";
        }
        std::cerr << "\n";
        std::cerr << "EXPECTED\n" << expectedS << "\n\n";
    }

    if (document.parseErrors.size() > 0) {
        std::cout << "ERR ";
        if (hasArgv)
            for (auto& err : document.parseErrors)
                std::cerr << err.msg << " " << err.range << "\n";
    }

    if (actualS == expectedS) {
        std::cout << "GOOD\n";
        return true;
    } else {
        std::cout << "BAD\n";
        return false;
    };
}

std::vector<std::string> nixpkgs_paths(nix::EvalState& state) {
    auto searchPath = state.getSearchPath();
    auto it = std::find_if(
        searchPath.elements.begin(),
        searchPath.elements.end(),
        [](const nix::SearchPath::Elem& e) { return e.prefix.s == "nixpkgs"; }
    );
    assert(it != searchPath.elements.end());
    std::string nixpkgs = it->path.s;
    std::vector<std::string> paths;
    for (auto f : std::filesystem::recursive_directory_iterator(nixpkgs)) {
        if (f.is_regular_file() && f.path().extension() == ".nix") {
            paths.push_back(f.path());
        }
    }
    return paths;
}

int main(int argc, char* argv[]) {
    nix::initGC();
    nix::initNix();
    auto state =
        std::make_unique<nix::EvalState>(nix::SearchPath{}, nix::openStore());

    std::vector<std::string> paths;
    if (argc >= 2) {
        hasArgv = true;
        for (int i = 1; i < argc; i++) {
            paths.push_back(argv[i]);
        }
    } else {
        paths = nixpkgs_paths(*state);
    }

    int total = paths.size();
    int good;

    std::for_each(
        std::execution::par,
        paths.begin(),
        paths.end(),
        [&](std::string path) { good += check_consistency(*state, path); }
    );

    std::cout << good << " / " << total << "\n";

    if (!hasArgv && good < currentProgress) {
        return 1;
    }

    if (hasArgv && good < total) {
        return 1;
    }
}
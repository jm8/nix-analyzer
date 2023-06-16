#include "config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "common/stringify.h"
#include "parser/parser.h"

// current progress:
// 26219 / 27150

bool verbose = false;

bool check_consistency(nix::EvalState& state, std::string path) {
    std::cout << path << " ";
    std::cout.flush();

    std::string source = nix::readFile(path);

    if (source.size() > 100000) {
        std::cout << "skipping\n";
        return false;
    }
    auto basePath = nix::absPath(nix::dirOf(path));

    auto analysis = parse(state, source, path, basePath, {});
    nix::Expr* actual = analysis.exprPath.back().e;

    nix::Expr* expected = state.parseExprFromString(source, basePath);

    auto actualS = stringify(state, actual);
    auto expectedS = stringify(state, expected);

    if (verbose) {
        std::cerr << "ACTUAL\n" << actualS << "\n";
        for (auto err : analysis.parseErrors) {
            std::cerr << err.message << " " << err.range << "\n";
        }
        std::cerr << "\n";
        std::cerr << "EXPECTED\n" << expectedS << "\n\n";
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
        searchPath.begin(),
        searchPath.end(),
        [](const nix::SearchPathElem& e) { return e.first == "nixpkgs"; }
    );
    assert(it != searchPath.end());
    std::string nixpkgs = it->second;
    std::vector<std::string> paths;
    for (auto f : std::filesystem::recursive_directory_iterator(nixpkgs)) {
        if (f.path().extension() == ".nix") {
            paths.push_back(f.path());
        }
    }
    return paths;
}

int main(int argc, char* argv[]) {
    nix::initGC();
    nix::initNix();
    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    int good = 0;
    int total = 0;

    std::vector<std::string> paths;
    if (argc >= 2) {
        verbose = true;
        for (int i = 1; i < argc; i++) {
            paths.push_back(argv[i]);
        }
    } else {
        paths = nixpkgs_paths(*state);
    }

    for (auto path : paths) {
        good += check_consistency(*state, path);
        total++;
    }

    std::cout << good << " / " << total << "\n";
}
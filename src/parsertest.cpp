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
#include "common/stringify.h"
#include "parser/parser.h"

bool check_consistency(nix::EvalState& state, std::string path) {
    std::cout << path << " ";
    std::cout.flush();
    std::string source = nix::readFile(path);
    auto basePath = nix::absPath(nix::dirOf(path));

    auto analysis = parse(state, source, path, basePath, {});
    nix::Expr* actual = analysis.exprPath.back().e;

    nix::Expr* expected = state.parseExprFromString(source, basePath);

    auto actualS = stringify(state, actual);
    auto expectedS = stringify(state, expected);

    if (actualS == expectedS) {
        std::cout << "good\n";
        return true;
    } else {
        std::cout << "bad\n";
        return false;
    };
}

int main(int argc, char* argv[]) {
    nix::initGC();
    nix::initNix();
    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    int good = 0;
    int total = 0;
    // std::string nixpkgs = state->findFile("nixpkgs");
    auto searchPath = state->getSearchPath();
    auto it = std::find_if(
        searchPath.begin(),
        searchPath.end(),
        [](const nix::SearchPathElem& e) { return e.first == "nixpkgs"; }
    );
    assert(it != searchPath.end());
    std::string nixpkgs = it->second;

    for (auto f : std::filesystem::recursive_directory_iterator(nixpkgs)) {
        if (f.path().extension() != ".nix")
            continue;
        good += check_consistency(*state, f.path());
        total++;
    }

    std::cout << good << " / " << total << "\n";
}
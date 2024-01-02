#include "na_config.h"
// #include <nix/eval.hh>
// #include <nix/nixexpr.hh>

// #include <nix/shared.hh>
// #include <nix/store-api.hh>
// #include <nix/types.hh>
// #include <nix/util.hh>
// #include <nix/value.hh>
// #include <algorithm>
// #include <filesystem>
// #include <fstream>
// #include <iostream>
// #include <memory>
// #include <string>
// #include <vector>
// #include "common/stringify.h"
// #include "parser/parser.h"

// int currentProgress = 26996;

// bool hasArgv = false;

// bool check_consistency(nix::EvalState& state, std::string path) {
//     std::cout << path << " ";
//     std::cout.flush();

//     std::string source = nix::readFile(path);

//     if (source.size() > 100000) {
//         std::cout << "skipping\n";
//         return false;
//     }
//     auto basePath = nix::absPath(nix::dirOf(path));

//     auto analysis = parse(state, source, path, basePath, {});
//     nix::Expr* actual = analysis.exprPath.back().e;

//     nix::Expr* expected = state.parseExprFromString(source, basePath);

//     auto actualS = stringify(state, actual);
//     auto expectedS = stringify(state, expected);

//     if (hasArgv) {
//         std::cerr << "ACTUAL\n" << actualS << "\n";
//         for (auto err : analysis.parseErrors) {
//             std::cerr << err.message << " " << err.range << "\n";
//         }
//         std::cerr << "\n";
//         std::cerr << "EXPECTED\n" << expectedS << "\n\n";
//     }

//     if (actualS == expectedS) {
//         std::cout << "GOOD\n";
//         return true;
//     } else {
//         std::cout << "BAD\n";
//         return false;
//     };
// }

// std::vector<std::string> nixpkgs_paths(nix::EvalState& state) {
//     auto searchPath = state.getSearchPath();
//     auto it = std::find_if(
//         searchPath.begin(),
//         searchPath.end(),
//         [](const nix::SearchPathElem& e) { return e.first == "nixpkgs"; }
//     );
//     assert(it != searchPath.end());
//     std::string nixpkgs = it->second;
//     std::vector<std::string> paths;
//     for (auto f : std::filesystem::recursive_directory_iterator(nixpkgs)) {
//         if (f.path().extension() == ".nix") {
//             paths.push_back(f.path());
//         }
//     }
//     return paths;
// }

#include <iostream>
int main(int argc, char* argv[]) {
    std::cout << "Hello from parsertest!\n";
}
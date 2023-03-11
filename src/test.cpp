#include "config.h"
#include <nix/eval.hh>
#include <nix/pos.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <any>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include "common/position.h"
#include "parser/parser.h"

bool runParseTest(nix::EvalState& state, nix::Value& v) {
    std::string source{state.forceString(
        *v.attrs->get(state.symbols.create("source"))->value, nix::noPos
    )};

    std::string path = "";

    auto analysis = parse(state, source, path, nix::absPath("."), {0, 0});

    auto expected = state.forceString(
        *v.attrs->get(state.symbols.create("expected"))->value, nix::noPos
    );
    if (expected.ends_with('\n')) {
        expected = expected.substr(0, expected.size() - 1);
    }
    std::stringstream ss;
    analysis.exprPath.back().e->show(state.symbols, ss);
    auto actual = ss.str();

    if (actual != expected) {
        std::cout << "EXPECTED: " << expected << "\n";
        std::cout << "ACTUAL: " << actual << "\n";
        std::cout << "FAIL\n";
        return false;
    }

    std::cout << "GOOD\n";

    return true;
}

bool runTest(nix::EvalState& state, std::string path) {
    auto v = state.allocValue();
    state.evalFile(nix::absPath(path), *v);
    state.forceAttrs(*v, nix::noPos);

    auto type = state.forceString(
        *v->attrs->get(state.symbols.create("type"))->value, nix::noPos
    );

    if (type == "parse") {
        return runParseTest(state, *v);
    }
    abort();
}

int main(int argc, char* argv[]) {
    nix::initNix();
    nix::initGC();

    auto state =
        std::make_unique<nix::EvalState>(nix::Strings{}, nix::openStore());

    int successCount = 0;
    int totalCount = 0;
    for (int i = 1; i < argc; i++) {
        std::cout << argv[i] << "\n";
        successCount += runTest(*state, argv[i]);
        totalCount++;
    }

    if (successCount == totalCount) {
        std::cout << "ALL GOOD\n";
        return 0;
    } else {
        std::cout << "A TEST FAILED\n";
        return 1;
    };
}
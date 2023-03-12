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
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "common/position.h"
#include "gtest/gtest.h"
#include "parser/parser.h"

class NixAnalyzerTest : public testing::TestWithParam<std::string> {
   protected:
    static void SetUpTestSuite() {
        nix::initGC();
        nix::initNix();
        state = new nix::EvalState(nix::Strings{}, nix::openStore());
    }

    static void TearDownTestSuite() { delete state; }

    static nix::EvalState* state;
};

nix::EvalState* NixAnalyzerTest::state = nullptr;

void runParseTest(nix::EvalState* state, nix::Value* v) {
    std::string source{state->forceString(
        *v->attrs->get(state->symbols.create("source"))->value, nix::noPos
    )};

    std::string path = "";

    auto analysis = parse(*state, source, path, nix::absPath("."), {0, 0});
    ASSERT_FALSE(analysis.exprPath.empty());

    auto expected = state->forceString(
        *v->attrs->get(state->symbols.create("expected"))->value, nix::noPos
    );
    if (expected.ends_with('\n')) {
        expected = expected.substr(0, expected.size() - 1);
    }

    auto vExpectedErrors =
        v->attrs->get(state->symbols.create("expectedErrors"))->value;
    state->forceList(*vExpectedErrors, nix::noPos);
    std::vector<std::string> expectedErrors;
    expectedErrors.reserve(vExpectedErrors->listSize());
    for (auto el : vExpectedErrors->listItems()) {
        expectedErrors.push_back(std::string(state->forceString(*el)));
    }

    std::vector<std::string> actualErrors;
    for (auto parseError : analysis.parseErrors) {
        std::stringstream ss;
        ss << parseError.message << " " << parseError.range;
        actualErrors.push_back(ss.str());
    }

    if (expected.ends_with('\n')) {
        expected = expected.substr(0, expected.size() - 1);
    }
    std::stringstream ss;
    analysis.exprPath.back().e->show(state->symbols, ss);
    auto actual = ss.str();

    ASSERT_EQ(actual, expected) << source;
    ASSERT_EQ(actualErrors, expectedErrors) << source;
}

TEST_P(NixAnalyzerTest, Works) {
    auto v = state->allocValue();
    state->evalFile(nix::absPath(GetParam()), *v);
    state->forceAttrs(*v, nix::noPos);

    auto type = state->forceString(
        *v->attrs->get(state->symbols.create("type"))->value, nix::noPos
    );

    ASSERT_EQ(type, "parse");
    ASSERT_NO_FATAL_FAILURE(runParseTest(state, v));
}

std::vector<std::string> arguments;

INSTANTIATE_TEST_SUITE_P(
    Instantiation,
    NixAnalyzerTest,
    testing::ValuesIn(arguments)
);

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++)
        arguments.push_back(argv[i]);

    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
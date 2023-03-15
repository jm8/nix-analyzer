#include "config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/shared.hh>
#include <nix/store-api.hh>
#include <nix/types.hh>
#include <nix/util.hh>
#include <nix/value.hh>
#include <any>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "common/position.h"
#include "common/stringify.h"
#include "gtest/gtest.h"
#include "parser/parser.h"

class FileTest : public testing::TestWithParam<std::string> {
   public:
    static void SetUpTestSuite() {
        nix::initGC();
        nix::initNix();
        state = new nix::EvalState(nix::Strings{}, nix::openStore());
    }

    static void TearDownTestSuite() { delete state; }

    static nix::EvalState* state;
};

nix::EvalState* FileTest::state = nullptr;

bool hasAttr(nix::EvalState* state, nix::Value* v, std::string_view key) {
    return v->attrs->get(state->symbols.create(key));
}

std::string getString(
    nix::EvalState* state,
    nix::Value* v,
    std::string_view key
) {
    auto i = v->attrs->get(state->symbols.create(key));
    if (!i) {
        std::cerr << "Missing key: " << key << "\n";
        abort();
    }
    return std::string{state->forceString(*i->value, nix::noPos)};
}

std::vector<std::string> getListOfStrings(
    nix::EvalState* state,
    nix::Value* v,
    std::string_view key
) {
    auto i = v->attrs->get(state->symbols.create(key));
    if (!i) {
        std::cerr << "Missing key: " << key << "\n";
        abort();
    }
    state->forceList(*i->value, nix::noPos);
    std::vector<std::string> list;
    list.reserve(i->value->listSize());
    for (auto el : i->value->listItems()) {
        list.push_back(std::string(state->forceString(*el)));
    }
    return list;
}

int getInt(nix::EvalState* state, nix::Value* v, std::string_view key) {
    auto i = v->attrs->get(state->symbols.create(key));
    if (!i) {
        std::cerr << "Missing key: " << key << "\n";
        abort();
    }
    return state->forceInt(*i->value, nix::noPos);
}

void runParseTest(nix::EvalState* state, nix::Value* v) {
    auto source = getString(state, v, "source");

    std::string path = "";

    Position targetPos{
        static_cast<uint32_t>(getInt(state, v, "line")),
        static_cast<uint32_t>(getInt(state, v, "col")),
    };

    auto analysis = parse(*state, source, path, nix::absPath("."), targetPos);
    ASSERT_FALSE(analysis.exprPath.empty()) << source;

    auto expected = getString(state, v, "expected");
    if (expected.ends_with('\n')) {
        expected = expected.substr(0, expected.size() - 1);
    }

    auto expectedErrors = getListOfStrings(state, v, "expectedErrors");

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

    auto expectedExprPath = getListOfStrings(state, v, "expectedExprPath");
    std::vector<std::string> actualExprPath;
    actualExprPath.reserve(analysis.exprPath.size());
    for (auto e : analysis.exprPath) {
        actualExprPath.push_back(exprTypeName(e.e));
    }

    if (hasAttr(state, v, "expectedAttrPath")) {
        ASSERT_TRUE(analysis.attr.has_value());
        auto expectedAttrPath = getString(state, v, "expectedAttrPath");
        auto actualAttrPath =
            nix::showAttrPath(state->symbols, *analysis.attr->attrPath);
        auto expectedAttrPathIndex = getInt(state, v, "expectedAttrPathIndex");
        auto actualAttrPathIndex = analysis.attr->index;

        ASSERT_EQ(actualAttrPath, expectedAttrPath) << source;
        ASSERT_EQ(actualAttrPathIndex, expectedAttrPathIndex) << source;
    } else {
        ASSERT_FALSE(analysis.attr.has_value()) << source;
    }

    if (hasAttr(state, v, "expectedFormal")) {
        ASSERT_TRUE(analysis.formal.has_value());
        auto expectedFormal = getString(state, v, "expectedFormal");
        std::string actualFormal = state->symbols[analysis.formal->name];
        ASSERT_EQ(actualFormal, expectedFormal);
    } else {
        ASSERT_FALSE(analysis.formal.has_value());
    }

    if (hasAttr(state, v, "expectedArg")) {
        ASSERT_TRUE(analysis.arg);
    } else {
        ASSERT_FALSE(analysis.arg);
    }

    ASSERT_EQ(actual, expected) << source;
    ASSERT_EQ(actualErrors, expectedErrors) << source;
    ASSERT_EQ(actualExprPath, expectedExprPath) << source;
}

TEST_P(FileTest, A) {
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
    A,
    FileTest,
    testing::ValuesIn(arguments),
    [](auto info) {
        auto path = static_cast<std::string>(info.param);
        auto start = path.find('/');
        auto end = path.find('.');
        return path.substr(start + 1, end - start - 1);
    }
);

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++)
        arguments.push_back(argv[i]);

    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}
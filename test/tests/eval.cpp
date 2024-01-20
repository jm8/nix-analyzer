#include <nix/eval.hh>
#include <catch2/catch.hpp>
#include <memory>
#include <vector>
#include "parser/parser.h"
#include "test.h"

extern std::unique_ptr<nix::EvalState> stateptr;

TEST_CASE("parent and staticenv") {
    auto& state = *stateptr;
    auto document = parse(state, path("/default.nix"), "let x = 4; in x");

    auto let = dynamic_cast<nix::ExprLet*>(document.root);
    auto var = let->body;

    REQUIRE(document.exprData[var].parent.value() == let);
}

TEST_CASE("basic env") {
    auto& state = *stateptr;
    auto document = parse(state, path("/default.nix"), "let x = 4; in x");

    auto let = dynamic_cast<nix::ExprLet*>(document.root);
    auto var = let->body;

    auto se = document.getStaticEnv(var);
    auto it = se->find(state.symbols.create("x"));
    REQUIRE(it != se->vars.end());
    auto env = document.getEnv(var);
    REQUIRE(env->values[it->second]->integer == 4);
}
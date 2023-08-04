#include "na_config.h"
#include "evaluateFlake.h"
#include <nix/eval.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/lockfile.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/value.hh>
#include <iostream>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/position.h"
#include "common/stringify.h"
#include "evaluation/evaluation.h"

nix::Value* evaluateFlake(
    nix::EvalState& state,
    nix::Expr* flake,
    std::optional<std::string_view> lockFile,
    std::vector<Diagnostic>& diagnostics
) {
    auto v = state.allocValue();
    v->mkNull();

    auto& env = state.baseEnv;

    auto flakeAttrs = dynamic_cast<nix::ExprAttrs*>(flake);

    if (!flakeAttrs) {
        diagnostics.push_back(
            {"must be an attribute set", Location(state, flake).range}
        );
        return v;
    }

    const auto sDescription = state.symbols.create("description");
    const auto sInputs = state.symbols.create("inputs");
    const auto sOutputs = state.symbols.create("outputs");

    bool outputsFound = false;
    for (auto [symbol, attr] : flakeAttrs->attrs) {
        if (symbol == sDescription) {
            auto vDescription =
                evaluateWithDiagnostics(state, attr.e, env, diagnostics);
            if (vDescription->type() != nix::nString) {
                diagnostics.push_back(
                    {"expected a string", Location{state, attr.e}.range}
                );
                continue;
            }
        }
        if (symbol == sInputs) {
            std::cerr << "encounter sInputs"
                      << "\n";
            auto inputsAttrs = dynamic_cast<nix::ExprAttrs*>(attr.e);
            if (!inputsAttrs) {
                diagnostics.push_back(
                    {"expected an attribute set", Location{state, attr.e}.range}
                );
                continue;
            }
            auto subEnv = updateEnv(state, flake, inputsAttrs, &env, {});

            for (auto [inputName, inputAttr] : inputsAttrs->attrs) {
                auto inputValue = evaluateWithDiagnostics(
                    state, inputAttr.e, *subEnv, diagnostics
                );
                std::cerr << "parsing flake input "
                          << stringify(state, inputValue) << "\n";
                try {
                    auto flakeInput = nix::flake::parseFlakeInput(
                        state,
                        state.symbols[inputName],
                        inputValue,
                        nix::noPos,
                        {},
                        {"root"}
                    );
                } catch (nix::Error& err) {
                    diagnostics.push_back(
                        {nix::filterANSIEscapes(err.msg(), true),
                         Location{state, inputAttr.e}.range}
                    );
                }
            }
        }
    }

    return v;
}
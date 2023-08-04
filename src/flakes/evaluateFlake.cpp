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
    std::optional<std::string_view> lockFileSource,
    std::vector<Diagnostic>& diagnostics
) {
    using namespace nix::flake;
    // 1. PARSE FLAKE FILE based on function getFlake

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

    std::map<nix::FlakeId, FlakeInput> inputs;

    const auto sDescription = state.symbols.create("description");
    const auto sInputs = state.symbols.create("inputs");
    const auto sOutputs = state.symbols.create("outputs");
    const auto sNixConfig = state.symbols.create("nixConfig");

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
        } else if (symbol == sInputs) {
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
                    auto flakeInput = parseFlakeInput(
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
        } else if (symbol == sOutputs) {
            auto subEnv = updateEnv(state, flake, attr.e, &env, {});
            auto function =
                evaluateWithDiagnostics(state, attr.e, *subEnv, diagnostics);
            if (function->type() != nix::nFunction) {
                diagnostics.push_back(
                    {"expected a function", Location{state, attr.e}.range}
                );
                continue;
            }
        } else if (symbol == sNixConfig) {
            // ignore for now
        } else {
            diagnostics.push_back(
                {"unsupported attribute '" + state.symbols[symbol] + "'",
                 Location{state, attr.e}.range}
            );
        }
    }

    // 2. GENERATE IN-MEMORY LOCK FILE based on lockFlake

    std::map<InputPath, FlakeInput> overrides;
    std::set<InputPath> overridesUsed, updatesUsed;

    LockFile oldLockFile;
    if (lockFileSource) {
        oldLockFile = LockFile{*lockFileSource, ""};
    }
    LockFile newLockFile;
}
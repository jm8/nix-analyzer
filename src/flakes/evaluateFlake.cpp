#include "na_config.h"
#include "evaluateFlake.h"
#include <nix/eval.hh>
#include <nix/fetchers.hh>
#include <nix/flake/flake.hh>
#include <nix/flake/flakeref.hh>
#include <nix/flake/lockfile.hh>
#include <nix/nixexpr.hh>
#include <nix/pos.hh>
#include <nix/url.hh>
#include <nix/value.hh>
#include <iostream>
#include <string_view>
#include <vector>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/evalutil.h"
#include "common/logging.h"
#include "common/position.h"
#include "common/stringify.h"
#include "evaluation/evaluation.h"

void computeFlakeDiagnostics(
    nix::EvalState& state,
    std::string_view path,
    nix::Expr* flakeExpr,
    std::vector<Diagnostic>& diagnostics
) {
    auto& env = state.baseEnv;

    auto flakeAttrs = dynamic_cast<nix::ExprAttrs*>(flakeExpr);

    if (!flakeAttrs) {
        diagnostics.push_back(
            {"must be an attribute set", Location(state, flakeExpr).range}
        );
        return;
    }

    std::map<nix::FlakeId, nix::flake::FlakeInput> inputs;

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
            auto subEnv = updateEnv(state, flakeExpr, inputsAttrs, &env, {});

            for (auto [inputName, inputAttr] : inputsAttrs->attrs) {
                auto inputValue = evaluateWithDiagnostics(
                    state, inputAttr.e, *subEnv, diagnostics
                );
                try {
                    auto flakeInput = nix::flake::parseFlakeInput(
                        state,
                        state.symbols[inputName],
                        inputValue,
                        nix::noPos,
                        {},
                        {"root"}
                    );
                    inputs.emplace(state.symbols[symbol], flakeInput);
                } catch (nix::Error& err) {
                    diagnostics.push_back(
                        {nix::filterANSIEscapes(err.msg(), true),
                         Location{state, inputAttr.e}.range}
                    );
                }
            }
        } else if (symbol == sOutputs) {
            auto subEnv = updateEnv(state, flakeExpr, attr.e, &env, {});
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
}

std::optional<std::string> lockFlake(
    nix::EvalState& state,
    std::string_view path
) {
    try {
        // TODO: this uses the saved flake.nix, not what is currently
        // in the editor.
        nix::fetchers::Attrs attrs;
        attrs.insert_or_assign("type", "path");
        std::string directoryPath(
            path.begin(), path.end() - std::string_view{"/flake.nix"}.length()
        );
        attrs.insert_or_assign("path", directoryPath);

        auto originalRef = nix::FlakeRef(
            nix::fetchers::Input::fromAttrs(std::move(attrs)), ""
        );

        auto [tree, lockedRef] = originalRef.fetchTree(state.store);
        state.allowPath(tree.storePath);

        nix::flake::LockFlags x;

        auto lockedFlake = nix::flake::lockFlake(
            state, lockedRef, nix::flake::LockFlags{.writeLockFile = false}
        );

        auto result = lockedFlake.lockFile.to_string();

        // this should download everything to the nix store on this thread so
        // that it will be fast for the other thread
        getFlakeLambdaArg(state, result);

        return result;
    } catch (nix::Error& err) {
        REPORT_ERROR(err);
        return {};
    }
}

nix::Value* getFlakeLambdaArg(
    nix::EvalState& state,
    std::string_view lockFile
) {
    try {
        auto vGetFlakeInputs = loadFile(state, "flakes/getFlakeInputs.nix");
        auto vRes = state.allocValue();
        auto vLockFileString = state.allocValue();
        vLockFileString->mkString(lockFile);

        state.callFunction(
            *vGetFlakeInputs, *vLockFileString, *vRes, nix::noPos
        );

        return vRes;
    } catch (nix::Error& err) {
        REPORT_ERROR(err);
        auto v = state.allocValue();
        v->mkNull();
        return v;
    }
}
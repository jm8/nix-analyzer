#include <nix/eval.hh>
#include <nix/nixexpr.hh>

nix::Env* updateEnv(nix::EvalState& state,
                    nix::Expr* parent,
                    nix::Expr* child,
                    nix::Env* up,
                    std::optional<nix::Value*> lambdaArg);

nix::Env* calculateEnv(nix::EvalState& state,
                       std::vector<nix::Expr*> exprPath,
                       std::vector<std::optional<nix::Value*>> lambdaArgs);
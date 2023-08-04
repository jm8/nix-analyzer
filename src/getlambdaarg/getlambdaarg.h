#pragma once
#include "na_config.h"
#include <nix/eval.hh>
#include "common/analysis.h"

void getLambdaArgs(nix::EvalState& state, Analysis& analysis);
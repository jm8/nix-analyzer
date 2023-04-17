#pragma once
#include "config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/symbol-table.hh>
#include <nix/value.hh>
#include <string>
#include <vector>
#include "hover/hover.h"

struct Analysis;

// A Schema represents what attrs should be in an attrset
struct Schema {
    // should be either a "type" (like in nixos modules) or an attrset mapping
    // to other Schemas
    nix::Value* value;

    std::vector<nix::Symbol> attrs(nix::EvalState& state);
    std::optional<HoverResult> hover(nix::EvalState& state);

    Schema attrSubschema(nix::EvalState& state, nix::Symbol symbol);
};

Schema getSchema(nix::EvalState& state, const Analysis& analysis);
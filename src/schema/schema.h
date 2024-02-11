#pragma once
#include "na_config.h"
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
    std::vector<nix::Value*> values;

    explicit Schema(std::vector<nix::Value*> values) : values(values) {}

    std::vector<nix::Symbol> attrs(nix::EvalState& state);
    std::optional<HoverResult> hover(nix::EvalState& state);

    Schema attrSubschema(nix::EvalState& state, nix::Symbol symbol);
    Schema functionSubschema(nix::EvalState& state);
    std::vector<Schema> nestedSchemas(nix::EvalState& state);
};

Schema getSchema(nix::EvalState& state, const Analysis& analysis);

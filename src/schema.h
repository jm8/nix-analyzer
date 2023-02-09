#pragma once

#include <string>
#include <variant>

#include "eval.hh"
#include "symbol-table.hh"
#include "value.hh"

#include <LibLsp/lsp/lsp_completion.h>

using NACompletionType = lsCompletionItemKind;

struct NACompletionItem {
    std::string text;
    std::optional<std::string> documentation;
};

enum class SchemaType {
    Options,
    Lambda,
    MkDerivation,
};

// a schema is used to provide completion for what attrs an attrsets should have
// if Schema is constructed from a Value*, it should be an option (_type =
// "option") or an atterset containing (attrsets that contain)* options.
// schemas can also be manually constructed from a vector of SchemaItem.
struct Schema {
    std::variant<nix::Value*,                    // options
                 std::vector<NACompletionItem>>  // function argument list
        rep;

    SchemaType type;

    Schema(nix::Value* options);
    Schema(std::vector<NACompletionItem> items);
    Schema(std::vector<NACompletionItem> items, SchemaType type);

    std::vector<NACompletionItem> getItems(nix::EvalState& state);

    std::optional<Schema> subschema(nix::EvalState& state, nix::Symbol symbol);
};

#include "mkderivation-schema.h"
#pragma once

#include <LibLsp/lsp/lsp_completion.h>
#include "value.hh"

#include <string>
#include <variant>

using NACompletionType = lsCompletionItemKind;

struct NACompletionItem {
    std::string text;
    std::optional<std::string> documentation;
};

// a schema is used to provide completion for what attrs an attrsets should have
// if Schema is constructed from a Value*, it should be an option (_type =
// "option") or an atterset containing (attrsets that contain)* options.
// schemas can also be manually constructed from a vector of SchemaItem.
struct Schema {
    std::variant<nix::Value*,                    // options
                 std::vector<NACompletionItem>>  // function argument list
        rep;

    Schema(nix::Value* options);
    Schema(std::vector<NACompletionItem> items);

    std::vector<NACompletionItem> getItems(nix::EvalState& state);
};

#include "mkderivation-schema.h"
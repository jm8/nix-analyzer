#include "schema.h"
#include <iostream>
#include <variant>
#include <vector>
#include "logger.h"
#include "nix-analyzer.h"
#include "value.hh"

using namespace std;
using namespace nix;

Schema::Schema(vector<NACompletionItem> items)
    : rep(items), type(SchemaType::Lambda) {
}

Schema::Schema(vector<NACompletionItem> items, SchemaType type)
    : rep(items), type(type) {
}

Schema::Schema(Value* options) : rep(options), type(SchemaType::Options) {
}

optional<Schema> Schema::subschema(EvalState& state, Symbol symbol) {
    if (holds_alternative<vector<NACompletionItem>>(rep)) {
        return {};
    }
    auto options = get<Value*>(rep);
    try {
        state.forceAttrs(*options, noPos);
    } catch (Error& e) {
        // log.info("Caught error: ", e.info().msg.str());
        return {};
    }
    if (auto typeAttr = options->attrs->get(state.symbols.create("type"))) {
        try {
            state.forceAttrs(*typeAttr->value, noPos);
        } catch (Error& e) {
            // log.info("Caught error: ", e.info().msg.str());
            return {};
        }
        if (auto nameAttr =
                typeAttr->value->attrs->get(state.symbols.create("name"))) {
            if (nameAttr->value->type() != nString)
                return {};
            if (string_view(nameAttr->value->string.s) == "attrsOf") {
                auto nestedTypesAttr = typeAttr->value->attrs->get(
                    state.symbols.create("nestedTypes"));
                if (!nestedTypesAttr)
                    return {};
                try {
                    state.forceAttrs(*nestedTypesAttr->value, noPos);
                } catch (Error& e) {
                    return {};
                }
                auto elemTypeAttr = nestedTypesAttr->value->attrs->get(
                    state.symbols.create("elemType"));
                if (!elemTypeAttr)
                    return {};
                try {
                    state.forceAttrs(*elemTypeAttr->value, noPos);
                } catch (Error& e) {
                    return {};
                }
                auto getSubOptionsAttr = elemTypeAttr->value->attrs->get(
                    state.symbols.create("getSubOptions"));
                if (!getSubOptionsAttr)
                    return {};
                try {
                    state.forceValue(*getSubOptionsAttr->value, noPos);
                } catch (Error& e) {
                    return {};
                }
                if (getSubOptionsAttr->value->type() != nix::nFunction) {
                    return {};
                }
                Value* arg = state.allocValue();
                arg->mkAttrs(state.allocBindings(0));
                Value* res = state.allocValue();
                state.callFunction(*getSubOptionsAttr->value, *arg, *res,
                                   noPos);
                return res;
                return {};
            }
        }
    }
    auto suboptionAttr = options->attrs->get(symbol);
    if (!suboptionAttr) {
        return {};
    }
    return suboptionAttr->value;
}

vector<NACompletionItem> Schema::getItems(nix::EvalState& state) {
    if (holds_alternative<Value*>(rep)) {
        auto options = get<Value*>(rep);

        try {
            state.forceAttrs(*options, noPos);
        } catch (Error& e) {
            return {};
        }

        if (options->attrs->get(state.symbols.create("type"))) {
            // todo: check if this is `submodule`
            return {};
        }

        vector<NACompletionItem> result;
        for (auto [symbol, pos, value] : *options->attrs) {
            auto str = string(state.symbols[symbol]);
            if (str.empty() || str[0] == '_') {
                continue;
            }
            result.push_back({str});
        }
        return result;
    } else if (holds_alternative<vector<NACompletionItem>>(rep)) {
        return get<vector<NACompletionItem>>(rep);
    } else {
        return {};
    }
}
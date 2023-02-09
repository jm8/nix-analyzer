#include "schema.h"
#include <variant>
#include "logger.h"
#include "nix-analyzer.h"
#include "value.hh"

using namespace std;
using namespace nix;

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
    auto suboptionAttr = options->attrs->get(symbol);
    if (!suboptionAttr) {
        return {};
    }
    return suboptionAttr->value;
}
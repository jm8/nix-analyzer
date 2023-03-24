#pragma once
#include "config.h"
#include <nix/eval.hh>
#include <istream>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <ostream>
#include <unordered_map>
#include "common/position.h"

struct ContentChange {
    std::optional<Range> range;
    std::string text;
};

inline void from_json(
    const nlohmann::json& json,
    ContentChange& contentChange
) {
    auto it = json.find("range");
    if (it != json.end()) {
        contentChange.range = it->get<Range>();
    }
    json.at("text").get_to(contentChange.text);
}

struct Document {
    std::string source;

    void applyContentChange(ContentChange contentChange);
};

struct LspServer {
    nix::EvalState& state;
    std::unordered_map<std::string, Document> documents;

    std::optional<std::string> hover(
        const Document& document,
        Position targetPos
    );

    void run(std::istream& in, std::ostream& out);
};
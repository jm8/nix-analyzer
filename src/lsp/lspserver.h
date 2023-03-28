#pragma once
#include "config.h"
#include <nix/eval.hh>
#include <istream>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <ostream>
#include <unordered_map>
#include "common/document.h"
#include "common/position.h"

struct LspServer {
    nix::EvalState& state;
    std::unordered_map<std::string, Document> documents;

    std::optional<std::string> hover(
        const Document& document,
        Position targetPos
    );

    void run(std::istream& in, std::ostream& out);
};
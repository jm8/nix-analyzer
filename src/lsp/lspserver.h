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
#include "lsp/jsonrpc.h"

struct LspServer {
    Connection conn;
    nix::EvalState& state;

    std::unordered_map<std::string, Document> documents;

    void run();
    void publishDiagnostics(const Document& document);
};
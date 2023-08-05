#pragma once
#include "na_config.h"
#include <nix/eval.hh>
#include <istream>
#include <msd/channel.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <ostream>
#include <unordered_map>
#include "common/document.h"
#include "common/position.h"
#include "lsp/jsonrpc.h"

struct FetcherInput {
    std::string uri;
    std::string source;
    std::string path;
};

struct FetcherOutput {
    std::string uri;
    std::string lockFileString;
};

struct LspServer {
    Connection conn;
    nix::EvalState& state;

    msd::channel<FetcherInput> fetcherInputChannel;
    msd::channel<FetcherOutput> fetcherOutputChannel;

    std::unordered_map<std::string, Document> documents;

    void run();
    void publishDiagnostics(const Document& document);
};
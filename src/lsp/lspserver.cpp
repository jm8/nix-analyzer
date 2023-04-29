#include "config.h"
#include "lsp/lspserver.h"
#include <nix/error.hh>
#include <nix/util.hh>
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <sstream>
#include <variant>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/logging.h"
#include "common/position.h"
#include "common/stringify.h"
#include "completion/completion.h"
#include "getlambdaarg/getlambdaarg.h"
#include "hover/hover.h"
#include "lsp/jsonrpc.h"
#include "parser/parser.h"
#include "schema/schema.h"

using namespace nlohmann::json_literals;

Analysis analyze(
    nix::EvalState& state,
    const Document document,
    Position targetPos
) {
    auto analysis = parse(
        state, document.source, document.path, document.basePath, targetPos
    );
    analysis.exprPath.back().e->bindVars(state, state.staticBaseEnv);
    getLambdaArgs(state, analysis);
    calculateEnvs(state, analysis);
    for (auto i : analysis.exprPath) {
        std::cerr << exprTypeName(i.e) << " ";
    }
    std::cerr << "\n";
    return analysis;
}

std::vector<Diagnostic> computeDiagnostics(
    nix::EvalState& state,
    const Document& document
) {
    auto analysis = analyze(state, document, {});
    auto v = state.allocValue();
    auto result = analysis.parseErrors;
    try {
        analysis.exprPath.back().e->eval(
            state, *analysis.exprPath.back().env, *v
        );
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        Diagnostic d;
        Position pos;
        if (e.info().errPos) {
            pos.line = e.info().errPos->line - 1;
            pos.character = e.info().errPos->column - 1;
        }
        result.push_back(
            {nix::filterANSIEscapes(e.info().msg.str(), true),
             {{pos.line, 0}, {pos.line + 1, 0}}}
        );
    }
    // auto root = analysis.exprPath.back().e;
    return result;
}

void LspServer::run() {
    while (true) {
        Message message = conn.read();
        if (holds_alternative<Request>(message)) {
            auto request = get<Request>(message);
            std::cerr << "<-- request " << request.method << "\n";
            if (request.method == "initialize") {
                conn.write(Response{
                    request.id,
                    {
                        {
                            "capabilities",
                            {
                                {"textDocumentSync", 2 /* incremental */},
                                {"hoverProvider", true},
                                {"completionProvider",
                                 {
                                     {"triggerCharacters", {"."}},
                                 }},
                                {"diagnosticProvider",
                                 {
                                     {"interFileDependencies", false},
                                     {"workspaceDiagnostics", false},
                                 }},
                                {"definitionProvider", true},
                            },
                        },
                    },
                });
            } else if (request.method == "shutdown") {
                conn.write(Response{request.id, nlohmann::json::value_t::null});
            } else if (request.method == "textDocument/hover") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                const auto& document = documents[url];
                auto analysis = analyze(state, document, position);
                auto h = hover(state, analysis);
                if (!h) {
                    conn.write(Response{
                        request.id, nlohmann::json::value_t::null});
                } else {
                    conn.write(Response{
                        request.id,
                        {
                            {
                                "contents",
                                {
                                    {"kind", "markdown"},
                                    {"value", h->markdown},
                                },
                            },
                        },
                    });
                }
            } else if (request.method == "textDocument/definition") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                const auto& document = documents[url];
                auto analysis = analyze(state, document, position);
                auto h = hover(state, analysis);
                if (!h || !h->definitionPos) {
                    conn.write(Response{
                        request.id, nlohmann::json::value_t::null});
                } else {
                    conn.write(Response{request.id, *h->definitionPos});
                }
            } else if (request.method == "textDocument/completion") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                const auto& document = documents[url];
                auto analysis = analyze(state, document, position);
                auto c = completion(state, analysis);
                auto completionItems = nlohmann::json::array();
                for (auto item : c.items) {
                    completionItems.push_back({{"label", item}});
                }
                std::sort(
                    completionItems.begin(),
                    completionItems.end(),
                    [](const auto& a, const auto& b) {
                        return a["label"] < b["label"];
                    }
                );
                conn.write(Response{request.id, completionItems});
            } else if (request.method == "textDocument/diagnostic") {
                std::string uri = request.params["textDocument"]["uri"];
                const auto& document = documents[uri];
                auto diagnostics = computeDiagnostics(state, document);
                conn.write(Response{
                    request.id,
                    {{"kind", "full"}, {"items", diagnostics}},
                });
            }
        } else if (holds_alternative<Response>(message)) {
            std::cerr << "<-- response\n";
            auto response = get<Response>(message);
        } else if (holds_alternative<Notification>(message)) {
            auto notification = get<Notification>(message);
            std::cerr << "<-- notification " << notification.method << "\n";
            if (notification.method == "exit") {
                break;
            } else if (notification.method == "textDocument/didOpen") {
                std::string uri = notification.params["textDocument"]["uri"];
                std::string path =
                    uri.substr(std::string_view{"file://"}.size());
                documents[uri] = {
                    uri,
                    notification.params["textDocument"]["text"]
                        .get<std::string>(),
                    path,
                    nix::dirOf(path)};
            } else if (notification.method == "textDocument/didChange") {
                std::string uri = notification.params["textDocument"]["uri"];
                auto& document = documents[uri];
                for (auto contentChange :
                     notification.params["contentChanges"]) {
                    document.applyContentChange(contentChange);
                }
            }
        }
    }
}
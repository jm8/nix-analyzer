#include "config.h"
#include "lsp/lspserver.h"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <sstream>
#include <variant>
#include "calculateenv/calculateenv.h"
#include "common/analysis.h"
#include "common/position.h"
#include "common/stringify.h"
#include "completion/completion.h"
#include "hover/hover.h"
#include "lsp/jsonrpc.h"
#include "parser/parser.h"
#include "schema/schema.h"

using namespace nlohmann::json_literals;

Analysis analyze(nix::EvalState& state, Document document, Position targetPos) {
    auto analysis = parse(
        state, document.source, document.path, document.basePath, targetPos
    );
    analysis.exprPath.back().e->bindVars(state, state.staticBaseEnv);
    calculateEnvs(state, analysis);
    return analysis;
}

void LspServer::run(std::istream& in, std::ostream& out) {
    Connection conn(in, out);

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
                            },
                        },
                    },
                });
            } else if (request.method == "shutdown") {
                conn.write(Response{request.id, nlohmann::json::value_t::null});
            } else if (request.method == "textDocument/hover") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                auto document = documents[url];
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
                                    {"value", *h},
                                },
                            },
                        },
                    });
                }
            } else if (request.method == "textDocument/completion") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                auto document = documents[url];
                auto analysis = analyze(state, document, position);
                auto c = completion(state, analysis);
                auto completionItems = nlohmann::json::array_t{};
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
                documents[uri] = {notification.params["textDocument"]["text"]
                                      .get<std::string>()};
                std::cerr << documents[uri].source << "\n";
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
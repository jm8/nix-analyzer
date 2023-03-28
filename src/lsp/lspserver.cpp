#include "config.h"
#include "lsp/lspserver.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <variant>
#include "common/stringify.h"
#include "hover/hover.h"
#include "lsp/jsonrpc.h"
#include "parser/parser.h"

using namespace nlohmann::json_literals;

std::optional<std::string> LspServer::hover(
    const Document& document,
    Position targetPos
) {
    auto analysis = parse(
        state, document.source, document.path, document.basePath, targetPos
    );
    return ::hover(state, analysis);
}

void LspServer::run(std::istream& in, std::ostream& out) {
    Connection conn(in, out);

    std::cerr << "Hello?\n";
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
                            },
                        },
                    },
                });
            } else if (request.method == "shutdown") {
                conn.write(Response{request.id, nlohmann::json::value_t::null});
            } else if (request.method == "textDocument/hover") {
                std::string url = request.params["textDocument"]["uri"];
                Position position = request.params["position"];
                auto h = hover(documents[url], position);
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
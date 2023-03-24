#include "config.h"
#include "lsp/lspserver.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <variant>
#include "lsp/jsonrpc.h"

using namespace nlohmann::json_literals;

// https://github.com/llvm/llvm-project/blob/b8576086c78a5aebf056a8fc8cc716dfee40b72e/clang-tools-extra/clangd/SourceCode.cpp#L171
size_t findStartOfLine(std::string_view content, size_t line0indexed) {
    size_t startOfLine = 0;
    for (size_t i = 0; i < line0indexed; i++) {
        size_t nextNewLine = content.find('\n', startOfLine);
        if (nextNewLine == std::string_view::npos) {
            return 0;
        }
        startOfLine = nextNewLine + 1;
    }
    return startOfLine;
}

size_t positionToOffset(std::string_view content, Position p) {
    if (p.line < 0) {
        return std::string_view::npos;
    }
    if (p.character < 0) {
        return std::string_view::npos;
    }
    return findStartOfLine(content, p.line) + p.character;
}

void Document::applyContentChange(ContentChange contentChange) {
    if (!contentChange.range) {
        source = contentChange.text;
        return;
    }

    auto startIndex = positionToOffset(source, contentChange.range->start);
    auto endIndex = positionToOffset(source, contentChange.range->end);

    // todo: Add range length comparison to ensure the documents in sync
    // like clangd
    source.replace(startIndex, endIndex - startIndex, contentChange.text);

    std::cerr << source << "\n";
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
                            },
                        },
                    },
                });
            } else if (request.method == "shutdown") {
                conn.write(Response{request.id, nlohmann::json::value_t::null});
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
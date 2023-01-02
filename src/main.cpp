#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include <memory>
#include <network/uri.hpp>
#include <ostream>
#include <string_view>
#include <vector>

#include "LibLsp/JsonRpc/Condition.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/JsonRpc/TcpServer.h"
#include "LibLsp/JsonRpc/stream.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/general/exit.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/general/lsServerCapabilities.h"
#include "LibLsp/lsp/general/lsTextDocumentClientCapabilities.h"
#include "LibLsp/lsp/general/shutdown.h"
#include "LibLsp/lsp/lsMarkedString.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/workspace/execute_command.h"

using namespace std;

class DummyLog : public lsp::Log {
   public:
    std::ofstream file;

    DummyLog(string_view path) : file(path.data()) {
    }

    void log(Level level, const std::wstring& msg) {
        file << msg.c_str() << std::endl;
    }
    void log(Level level, std::wstring&& msg) {
        file << msg.c_str() << std::endl;
    }
    void log(Level level, std::string&& msg) {
        file << msg << std::endl;
    };
    void log(Level level, const std::string& msg) {
        file << msg << std::endl;
    };
};

TextDocumentHover::Either markdown(std::string markdown) {
    // lol
    return {{}, {{"markdown", markdown}}};
}

class NixLanguageServer {
   public:
    RemoteEndPoint remoteEndPoint;
    DummyLog log{"log.txt"};

    struct OStream : lsp::base_ostream<std::ostream> {
        explicit OStream(std::ostream& _t) : base_ostream<std::ostream>(_t) {
        }

        std::string what() override {
            return {};
        }
    };
    struct IStream : lsp::base_istream<std::istream> {
        explicit IStream(std::istream& _t) : base_istream<std::istream>(_t) {
        }

        std::string what() override {
            return {};
        }
    };

    NixLanguageServer()
        : remoteEndPoint(make_shared<lsp::ProtocolJsonHandler>(),
                         make_shared<GenericEndpoint>(log),
                         log) {
        remoteEndPoint.registerHandler([&](const td_initialize::request& req) {
            td_initialize::response res;
            res.id = req.id;
            log.info("initialize");
            res.result.capabilities.hoverProvider = true;
            res.result.capabilities.completionProvider = {{
                .resolveProvider = false,
                .triggerCharacters = {{".", "${"}},
            }};
            return res;
        });

        remoteEndPoint.registerHandler([&](Notify_Exit::notify& notify) {
            log.info("exit: " + notify.jsonrpc);
            remoteEndPoint.stop();
            esc_event.notify(make_unique<bool>(true));
        });

        remoteEndPoint.registerHandler([&](const td_hover::request& req) {
            log.info("hover");
            td_hover::response res;
            res.result.contents = markdown("Hello there");
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_completion::request& req) {
            td_completion::response res;
            res.result.items.push_back(lsCompletionItem{
                .label = "aaa",
                .kind = {lsCompletionItemKind::Function},
                .detail = {"Scream"},
            });
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_shutdown::request& req) {
            log.info("shutdown");
            td_shutdown::response res;
            return res;
        });

        remoteEndPoint.startProcessingMessages(make_shared<IStream>(cin),
                                               make_shared<OStream>(cout));
    }

    ~NixLanguageServer() {
    }

    Condition<bool> esc_event;
};

int main() {
    NixLanguageServer server;
    server.esc_event.wait();
}
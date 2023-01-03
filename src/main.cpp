#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>

#include <memory>
#include <network/uri.hpp>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
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
#include "LibLsp/lsp/general/initialized.h"
#include "LibLsp/lsp/general/lsServerCapabilities.h"
#include "LibLsp/lsp/general/lsTextDocumentClientCapabilities.h"
#include "LibLsp/lsp/general/shutdown.h"
#include "LibLsp/lsp/lsAny.h"
#include "LibLsp/lsp/lsDocumentUri.h"
#include "LibLsp/lsp/lsMarkedString.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/did_close.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/workspace/execute_command.h"

#include "nix-analyzer.h"
#include "store-api.hh"

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
    unique_ptr<NixAnalyzer> analyzer;
    // map from url to document content
    unordered_map<string, string> documents;

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

    NixLanguageServer(nix::Strings searchPath, nix::ref<nix::Store> store)
        : remoteEndPoint(make_shared<lsp::ProtocolJsonHandler>(),
                         make_shared<GenericEndpoint>(log),
                         log),
          analyzer(make_unique<NixAnalyzer>(searchPath, store)) {
        remoteEndPoint.registerHandler([&](const td_initialize::request& req) {
            td_initialize::response res;
            res.id = req.id;
            log.info("initialize");
            res.result.capabilities.hoverProvider = true;
            res.result.capabilities.completionProvider = {{
                .resolveProvider = false,
                .triggerCharacters = {{".", "${"}},
            }};
            res.result.capabilities.textDocumentSync = {
                {lsTextDocumentSyncKind::Full}, {}};
            return res;
        });

        remoteEndPoint.registerHandler(
            [&](Notify_InitializedNotification::notify& notify) {
                log.info("initialized");
            });

        remoteEndPoint.registerHandler([&](Notify_Exit::notify& notify) {
            log.info("exit: " + notify.jsonrpc);
            remoteEndPoint.stop();
            esc_event.notify(make_unique<bool>(true));
        });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidOpen::notify& notify) {
                string uri = notify.params.textDocument.uri.raw_uri_;
                log.info("didOpen: " + uri);
                documents[uri] = notify.params.textDocument.text;
            });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidChange::notify& notify) {
                string uri = notify.params.textDocument.uri.raw_uri_;
                log.info("didChange: " + uri);
                documents[uri] = notify.params.contentChanges[0].text;
            });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidClose::notify& notify) {
                string uri = notify.params.textDocument.uri.raw_uri_;
                log.info("didClose: " + uri);
                documents.erase(uri);
            });

        remoteEndPoint.registerHandler([&](const td_hover::request& req) {
            log.info("hover");
            td_hover::response res;
            res.result.contents = markdown("Hello there");
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_completion::request& req) {
            log.info("completion");
            td_completion::response res;
            string url = req.params.textDocument.uri.raw_uri_;
            auto it = documents.find(url);
            if (it == documents.end()) {
                return res;
            }
            string_view content{it->second};
            res.result.items.push_back(lsCompletionItem{
                .label = "aaa" + string(1, content[0]),
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
    nix::initNix();
    nix::initGC();
    nix::Strings searchPath;
    NixLanguageServer server(searchPath, nix::openStore());

    server.esc_event.wait();
}
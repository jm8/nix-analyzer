#include <boost/algorithm/string/find.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>

#include <memory>
#include <network/uri.hpp>
#include <ostream>
#include <sstream>
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
#include "LibLsp/lsp/utils.h"
#include "LibLsp/lsp/workspace/execute_command.h"

#include "error.hh"
#include "nix-analyzer.h"
#include "store-api.hh"
#include "util.hh"

#include "logger.h"

using namespace std;

TextDocumentHover::Either markdown(std::string markdown) {
    // lol
    return {{}, {{"markdown", markdown}}};
}

template <typename T>
string stringify(T x) {
    stringstream ss;
    ss << x;
    return ss.str();
}

class NixLanguageServer {
   public:
    RemoteEndPoint remoteEndPoint;
    lsp::Log& log;
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

    NixLanguageServer(nix::Strings searchPath,
                      nix::ref<nix::Store> store,
                      Logger& log)
        : remoteEndPoint(make_shared<lsp::ProtocolJsonHandler>(),
                         make_shared<GenericEndpoint>(log),
                         log),
          log(log),
          analyzer(make_unique<NixAnalyzer>(searchPath, store, log)) {
        remoteEndPoint.registerHandler([&](const td_initialize::request& req) {
            td_initialize::response res;
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
            res.result.contents = markdown("Yoo **wassup**");
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
            string source = it->second;

            string path = req.params.textDocument.uri.GetAbsolutePath().path;
            string basePath;
            if (path.empty() or lsp::StartsWith(path, "file://")) {
                return res;
            }
            basePath = nix::dirOf(path);

            nix::Pos pos{path, nix::foFile, req.params.position.line + 1,
                         req.params.position.character + 1};

            log.info(nix::filterANSIEscapes(stringify(pos), true));

            auto analysis = analyzer->getExprPath(source, path, basePath, pos);
            auto completions = analyzer->complete(analysis.exprPath,
                                                  {path, FileType::Package});

            auto toLsCompletionItemKind = [](CompletionItem::Type type) {
                if (type == CompletionItem::Type::Variable)
                    return lsCompletionItemKind::Variable;
                if (type == CompletionItem::Type::Property)
                    return lsCompletionItemKind::Property;
                return lsCompletionItemKind::Variable;
            };

            for (auto completion : completions) {
                res.result.items.push_back({
                    .label = completion.text,
                    .kind = {toLsCompletionItemKind(completion.type)},
                });
            }
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
    Logger log{};
    NixLanguageServer server(searchPath, nix::openStore(), log);

    server.esc_event.wait();
}
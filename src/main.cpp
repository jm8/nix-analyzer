#include <algorithm>
#include <boost/algorithm/string/find.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <cstddef>
#include <mutex>
#include <network/uri.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "LibLsp/JsonRpc/Condition.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/NotificationInMessage.h"
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
#include "LibLsp/lsp/location_type.h"
#include "LibLsp/lsp/lsAny.h"
#include "LibLsp/lsp/lsDocumentUri.h"
#include "LibLsp/lsp/lsMarkedString.h"
#include "LibLsp/lsp/lsPosition.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/did_close.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/textDocument/document_link.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/textDocument/formatting.h"
#include "LibLsp/lsp/textDocument/hover.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/utils.h"
#include "LibLsp/lsp/workspace/execute_command.h"

#include "error.hh"
#include "eval.hh"
#include "logging.hh"
#include "nix-analyzer.h"
#include "nixexpr.hh"
#include "store-api.hh"
#include "util.hh"

#include "logger.h"
#include "value.hh"

using namespace std;

TextDocumentHover::Either hoverMarkdown(std::string markdown) {
    // lol
    return {{}, {{"markdown", markdown}}};
}

std::optional<
    std::pair<std::optional<std::string>, std::optional<MarkupContent>>>
docMarkdown(std::string markdown) {
    // lol
    return {{{}, {{"markdown", markdown}}}};
}

template <typename T>
string stringify(T x) {
    stringstream ss;
    ss << x;
    return ss.str();
}

struct LspLogIgnore : lsp::Log {
    void log(Level level, std::wstring&& msg){};
    void log(Level level, const std::wstring& msg){};
    void log(Level level, std::string&& msg){};
    void log(Level level, const std::string& msg){};
};

LspLogIgnore lsplogignore;

struct Document {
    string text;
    FileInfo fileinfo;
};

class NixLanguageServer {
   public:
    RemoteEndPoint remoteEndPoint;
    unique_ptr<NixAnalyzer> analyzer;
    // prevent other things from happening while contentChange is happening.
    // guh
    mutex contentChangeMutex;
    // map from uri to document content
    unordered_map<string, Document> documents;

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

    void publishDiagnostics(lsDocumentUri uri,
                            const vector<nix::ParseError> parseErrors) {
        // stop publishing parse errors cause they're not very good
    }

    size_t findStartOfLine(string_view content, size_t line0indexed) {
        size_t startOfLine = 0;
        for (size_t i = 0; i < line0indexed; i++) {
            size_t nextNewLine = content.find('\n', startOfLine);
            if (nextNewLine == string_view::npos) {
                na_log.dbg("Line value out of range");
                return 0;
            }
            startOfLine = nextNewLine + 1;
        }
        return startOfLine;
        // size_t n = 0;
        // while (content[startOfLine + n] != '\n') {
        //     n++;
        // }
        // return content.substr(startOfLine, n);
    }

    // https://github.com/llvm/llvm-project/blob/b8576086c78a5aebf056a8fc8cc716dfee40b72e/clang-tools-extra/clangd/SourceCode.cpp#L171
    size_t positionToOffset(string_view content, lsPosition p) {
        if (p.line < 0) {
            na_log.dbg("Negative line value");
            return string_view::npos;
        }
        if (p.character < 0) {
            na_log.dbg("Negative character value");
            return string_view::npos;
        }
        return findStartOfLine(content, p.line) + p.character;
    }

    // https://github.com/llvm/llvm-project/blob/b8576086c78a5aebf056a8fc8cc716dfee40b72e/clang-tools-extra/clangd/SourceCode.cpp#L1099
    void applyContentChange(string& content,
                            lsTextDocumentContentChangeEvent change) {
        if (!change.range) {
            content = change.text;
            return;
        }

        auto startIndex = positionToOffset(content, change.range->start);
        auto endIndex = positionToOffset(content, change.range->end);

        // todo: Add range length comparison to ensure the documents in sync
        // like clangd
        content.replace(startIndex, endIndex - startIndex, change.text);
    }

    std::optional<Analysis> getExprPath(
        lsDocumentUri uri,
        optional<pair<uint32_t, uint32_t>> position) {
        auto it = documents.find(uri.raw_uri_);
        if (it == documents.end()) {
            na_log.dbg("Document ", uri.raw_uri_, " does not exist");
            return {};
        }
        string source = it->second.text;

        nix::Path path{uri.raw_uri_};
        nix::Path basePath;
        if (lsp::StartsWith(path, "file://")) {
            path = path.substr(7);
            basePath = nix::dirOf(path);
        } else {
            na_log.dbg("Path does not have a base path: ", uri.raw_uri_);
            basePath = nix::absPath(".");
        }
        na_log.dbg("Base path of ", uri.raw_uri_, " is ", basePath);

        nix::Pos pos;
        if (position) {
            pos = {path, nix::foFile, position->first, position->second};
        } else {
            pos = {path, nix::foFile, 1, 1};
        }

        auto startOfLine = findStartOfLine(source, pos.line - 1);

        size_t n = 0;
        while (startOfLine + n < source.size() &&
               source[startOfLine + n] != '\n' && n < 80) {
            n++;
        }
        auto line = source.substr(startOfLine, n);

        na_log.dbg("Analyzing ", uri.raw_uri_, ":", pos.line, ":", pos.column);
        na_log.dbg(line);
        na_log.dbg(string(pos.column - 1, ' ') + '^');

        return analyzer->getExprPath(source, path, basePath, pos);
    }

    FileInfo getDocumentFileInfo(lsDocumentUri uri) {
        auto it = documents.find(uri.raw_uri_);
        if (it != documents.end()) {
            return it->second.fileinfo;
        } else {
            return FileInfo{uri.GetAbsolutePath().path};
        }
    }

    NixLanguageServer(nix::Strings searchPath, nix::ref<nix::Store> store)
        : remoteEndPoint(make_shared<lsp::ProtocolJsonHandler>(),
                         make_shared<GenericEndpoint>(lsplogignore),
                         lsplogignore),
          analyzer(make_unique<NixAnalyzer>(searchPath, store)) {
        remoteEndPoint.registerHandler([&](const td_initialize::request& req) {
            td_initialize::response res;
            na_log.dbg("initialize");
            res.result.capabilities.hoverProvider = true;
            res.result.capabilities.completionProvider = {{
                .resolveProvider = false,
                .triggerCharacters = {{".", "${"}},
            }};
            res.result.capabilities.textDocumentSync = {
                {lsTextDocumentSyncKind::Incremental}, {}};
            res.result.capabilities.documentFormattingProvider = {{{true}, {}}};
            res.result.capabilities.definitionProvider = {{{true}, {}}};
            return res;
        });

        remoteEndPoint.registerHandler(
            [&](Notify_InitializedNotification::notify& notify) {
                na_log.dbg("initialized");
            });

        remoteEndPoint.registerHandler([&](Notify_Exit::notify& notify) {
            na_log.dbg("exit: ", notify.jsonrpc);
            remoteEndPoint.stop();
            esc_event.notify(make_unique<bool>(true));
        });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidOpen::notify& notify) {
                auto uri = notify.params.textDocument.uri;
                na_log.dbg("didOpen: ", uri.raw_uri_);
                documents[uri.raw_uri_] = {
                    notify.params.textDocument.text,
                    FileInfo{uri.GetAbsolutePath().path}};
            });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidChange::notify& notify) {
                lock_guard<mutex> lock(contentChangeMutex);
                auto uri = notify.params.textDocument.uri;
                na_log.dbg("didChange: ", uri.raw_uri_);
                string& content = documents[uri.raw_uri_].text;
                for (auto contentChange : notify.params.contentChanges) {
                    applyContentChange(content, contentChange);
                }
            });

        remoteEndPoint.registerHandler(
            [&](Notify_TextDocumentDidClose::notify& notify) {
                string uri = notify.params.textDocument.uri.raw_uri_;
                na_log.dbg("didClose: ", uri);
                documents.erase(uri);
            });

        remoteEndPoint.registerHandler([&](const td_hover::request& req) {
            lock_guard<mutex> lock(contentChangeMutex);
            na_log.dbg("hover");
            td_hover::response res;
            auto analysis = getExprPath(req.params.textDocument.uri,
                                        {{req.params.position.line + 1,
                                          req.params.position.character + 1}});
            if (!analysis)
                return res;

            auto hoverResult = analyzer->hover(
                *analysis, getDocumentFileInfo(req.params.textDocument.uri));

            if (!hoverResult.text)
                return res;

            res.result.contents =
                hoverMarkdown("```nix\n" + *hoverResult.text + "\n```");
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_definition::request& req) {
            lock_guard<mutex> lock(contentChangeMutex);
            na_log.dbg("goto definition");
            td_definition::response res;
            res.result.first = {vector<lsLocation>{}};
            auto analysis = getExprPath(req.params.textDocument.uri,
                                        {{req.params.position.line + 1,
                                          req.params.position.character + 1}});
            if (!analysis)
                return res;

            auto hoverResult = analyzer->hover(
                *analysis, getDocumentFileInfo(req.params.textDocument.uri));

            if (!hoverResult.pos)
                return res;

            auto pos = *hoverResult.pos;

            lsLocation location;
            if (pos.file.find("untitled:") != string::npos) {
                location.uri.raw_uri_ = pos.file;
            } else {
                location.uri = lsDocumentUri{AbsolutePath{pos.file}};
            }
            na_log.dbg(location.uri.raw_uri_);
            location.range.start = {static_cast<int>(pos.line - 1),
                                    static_cast<int>(pos.column - 1)};
            location.range.end = location.range.start;
            res.result.first->push_back(location);
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_completion::request& req) {
            lock_guard<mutex> lock(contentChangeMutex);
            na_log.dbg("completion");
            td_completion::response res;
            auto uri = req.params.textDocument.uri;
            auto analysis =
                getExprPath(uri, {{req.params.position.line + 1,
                                   req.params.position.character + 1}});

            auto [completionType, completions] = analyzer->complete(
                *analysis, getDocumentFileInfo(req.params.textDocument.uri));
            if (req.params.context->triggerCharacter == "." &&
                completionType == NACompletionType::Variable) {
                na_log.dbg(
                    "Completion was triggered with `.` but the completion kind "
                    "is not Variable. Ignoring");
                return res;
            }
            for (auto completion : completions) {
                res.result.items.push_back({
                    .label = completion.text,
                    .kind = {completionType},
                    .documentation =
                        completion.documentation
                            ? docMarkdown(*completion.documentation)
                            : std::nullopt,
                    .commitCharacters = {{"."}},
                });
            }
            return res;
        });

        remoteEndPoint.registerHandler([&](const td_formatting::request& req) {
            namespace bp = boost::process;
            td_formatting::response res;

            string output;
            try {
                boost::asio::io_service ios;
                std::future<std::string> data;
                bp::child c(
                    // todo: make configurable
                    bp::search_path("alejandra"), "-qq",
                    (bp::std_in <
                     bp::buffer(
                         documents[req.params.textDocument.uri.raw_uri_].text)),
                    (bp::std_out > data), ios);
                ios.run();
                output = data.get();
            } catch (bp::process_error& e) {
                na_log.dbg("error running alejandra: ", e.what());
                return res;
            }

            res.result.push_back(lsTextEdit{
                lsRange{
                    {0, 0},
                    {999999, 999999},
                },
                output,
            });

            return res;
        });

        remoteEndPoint.registerHandler([&](const td_shutdown::request& req) {
            na_log.dbg("shutdown");
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
    nix::verbosity = nix::lvlVomit;
    nix::Strings searchPath;
    NixLanguageServer server(searchPath, nix::openStore());

    server.esc_event.wait();
}
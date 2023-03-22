#include "config.h"
#include "lsp/lspserver.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <variant>
#include "lsp/jsonrpc.h"

void LspServer::run(std::istream& in, std::ostream& out) {
    Connection conn(in, out);

    std::cerr << "Hello?\n";
    while (true) {
        Message message = conn.read();
        if (holds_alternative<Request>(message)) {
            auto request = get<Request>(message);
            std::cerr << "Request :" << request.params << "\n";
        } else if (holds_alternative<Response>(message)) {
            std::cerr << "Response\n";
        } else if (holds_alternative<Notification>(message)) {
            std::cerr << "Notification\n";
        }
    }
}
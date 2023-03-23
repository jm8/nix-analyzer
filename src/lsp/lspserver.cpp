#include "config.h"
#include "lsp/lspserver.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <variant>
#include "lsp/jsonrpc.h"

void LspServer::run(std::istream& in, std::ostream& out) {
    Connection conn(in, out);

    std::cerr << "Hello?\n";
    while (true) {
        Message message = conn.read();
        if (holds_alternative<Request>(message)) {
            auto request = get<Request>(message);
            std::cerr << "Request:" << request.method << "\n";
            conn.write(Response{
                request.id,
                {
                    {"capabilities", nlohmann::json::value_t::object},
                },
            });
        } else if (holds_alternative<Response>(message)) {
            auto response = get<Response>(message);
            // std::cerr << "<-- " << response;
        } else if (holds_alternative<Notification>(message)) {
            auto notification = get<Notification>(message);
            std::cerr << "<-- " << notification.method;
        }
    }
}
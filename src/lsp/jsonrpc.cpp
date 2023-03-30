#include "lsp/jsonrpc.h"
#include <iostream>
#include <nlohmann/json_fwd.hpp>
#include <variant>

Connection::Connection(std::istream& in, std::ostream& out)
    : in(in), out(out) {}

// Reads a json-rpc header returning the content-length
size_t Connection::read_header() {
    auto expect = [&](std::string_view s) {
        for (char c : s) {
            assert(in.get() == c);
        }
    };
    size_t content_length;
    while (in.peek() != '\r') {
        expect("Content-");
        if (in.peek() == 'L') {
            expect("Length:");
            in >> content_length;
        } else {
            expect("Type:");
            while (in.peek() != '\r') {
                in.get();
            }
        }
        expect("\r\n");
    }
    expect("\r\n");
    return content_length;
}

Message Connection::read() {
    size_t content_length = read_header();
    std::string content(content_length, '\0');
    in.read(content.data(), content_length);
    nlohmann::json json = nlohmann::json::parse(content);
    if (json.contains("id")) {
        if (json.contains("method")) {
            return json.get<Request>();
        } else {
            return json.get<Response>();
        }
    } else {
        return json.get<Notification>();
    }
}

void Connection::write(Message message) {
    auto json =
        std::visit([](auto&& x) -> nlohmann::json { return x; }, message);
    json["jsonrpc"] = "2.0";
    auto content = json.dump();
    std::cerr << "--> response\n";
    // std::cerr << "--> " << content << "\n";
    out << "Content-Length: " << content.length() << "\r\n";
    out << "\r\n";
    out << content;
}
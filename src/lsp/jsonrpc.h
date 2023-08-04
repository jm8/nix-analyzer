#pragma once
#include "na_config.h"
#include <cassert>
#include <cstddef>
#include <istream>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <ostream>
#include <variant>

// based on
// https://github.com/LPeter1997/LSP-Project/

struct Request {
    nlohmann::json id;
    std::string method;
    nlohmann::json params;
};

struct Response {
    nlohmann::json id;
    nlohmann::json result;
};

struct Notification {
    std::string method;
    nlohmann::json params;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Request, id, method, params)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Response, id, result)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Notification, method, params)

using Message = std::variant<Request, Response, Notification>;

struct Connection {
    std::istream& in;
    std::ostream& out;

    Connection(std::istream& in, std::ostream& out);

    // Reads a json-rpc header returning the content-length
    size_t read_header();

    Message read();

    void write(Message message);
};
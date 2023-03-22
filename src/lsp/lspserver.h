#pragma once
#include "config.h"
#include <istream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <ostream>

class LspServer {
   public:
    void run(std::istream& in, std::ostream& out);
};
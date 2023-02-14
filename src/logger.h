#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include "LibLsp/JsonRpc/MessageIssue.h"

class Logger {
   public:
    std::ofstream file;

    Logger(std::string_view path);

    // called dbg to not conflict with nix's debug() macro
    template <typename... T>
    void dbg(T... args) {
        ((std::cerr << args), ...);
        std::cerr << "\n";
        ((file << args), ...);
        file << "\n";
    }

    template <typename... T>
    void warning(T... args) {
        ((std::cerr << args), ...);
        std::cerr << "\n";
        ((file << args), ...);
        file << "\n";
    }
};

extern Logger na_log;
#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include "LibLsp/JsonRpc/MessageIssue.h"

class Logger : public lsp::Log {
   public:
    std::ofstream file;

    Logger(std::string_view path);

    void log(Level level, const std::wstring& msg);
    void log(Level level, std::wstring&& msg);
    void log(Level level, std::string&& msg);
    void log(Level level, const std::string& msg);

    template <typename... T>
    void info(T... args) {
        std::stringstream ss;
        ((ss << args), ...);
        log(Level::INFO, ss.str());
    }
};
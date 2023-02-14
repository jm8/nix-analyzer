#include "logger.h"
#include <boost/filesystem.hpp>
#include "util.hh"

Logger::Logger(std::string_view path) : file(path.data()) {
}

std::string getLoggerPath() {
    std::string cacheDir = nix::getHome() + "/.cache/nix-analyzer";
    boost::filesystem::create_directories(cacheDir);
    return cacheDir + "/nix-analyzer.log";
}

Logger na_log{getLoggerPath()};
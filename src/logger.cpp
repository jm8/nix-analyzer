#include "logger.h"
#include <boost/filesystem.hpp>
#include "util.hh"
using namespace std;

Logger::Logger(std::string_view path) : file(path.data()) {
}

void Logger::log(Level level, const std::wstring& msg) {
    file << msg.data() << std::endl;
}
void Logger::log(Level level, std::wstring&& msg) {
    file << msg.data() << std::endl;
}
void Logger::log(Level level, std::string&& msg) {
    log(level, msg);
};
void Logger::log(Level level, const std::string& msg) {
    file << nix::filterANSIEscapes(msg, true) << std::endl;
};

Logger::Logger() {
    string cacheDir = nix::getHome() + "/.cache/nix-analyzer"s;
    boost::filesystem::create_directories(cacheDir);
    file = ofstream{(cacheDir + "/nix-analyzer.log").data()};
}
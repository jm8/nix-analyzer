#include "logger.h"
#include <boost/filesystem.hpp>
#include "util.hh"
using namespace std;

Logger::Logger(string_view path) : file(path.data()) {
}

void Logger::log(Level level, const wstring& msg) {
    file << msg.data() << endl;
    cerr << msg.data() << endl;
}
void Logger::log(Level level, wstring&& msg) {
    file << msg.data() << endl;
    cerr << msg.data() << endl;
}
void Logger::log(Level level, string&& msg) {
    log(level, msg);
};
void Logger::log(Level level, const string& msg) {
    file << nix::filterANSIEscapes(msg, true) << endl;
    cerr << nix::filterANSIEscapes(msg, true) << endl;
};
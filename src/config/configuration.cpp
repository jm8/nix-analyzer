
#include "configuration.h"
#include <common/logging.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>
#include "util.hh"
#include "value.hh"

namespace fs = std::filesystem;

const std::string CONFIG_FILE_NAME = "nix-analyzer-config.nix";

std::optional<nix::Value*> tryLoad(nix::EvalState& state, fs::path path) {
    if (!fs::exists(path)) return {};
    auto v = state.allocValue();
    try {
        state.evalFile(path, *v);
        std::cerr << "Loaded config file " << path << "\n";
    } catch (nix::Error& e) {
        REPORT_ERROR(e);
        return {};
    }
    return {v};
}

// Load the configuration files that will be used for evaluating the file at
// path
Config Config::load(nix::EvalState& state, std::string path) {
    Config config;
    auto curr_dir = fs::path{path}.parent_path();
    while (true) {
        if (auto v = tryLoad(state, curr_dir / CONFIG_FILE_NAME)) {
            config.values.push_back(*v);
        }
        auto parent_dir = curr_dir.parent_path();
        if (parent_dir == curr_dir)
            break;
        curr_dir = parent_dir;
    }

    for (auto dir_s : nix::getConfigDirs()) {
        fs::path dir{dir_s};
        fs::path file = dir / CONFIG_FILE_NAME;
        if (auto v = tryLoad(state, file)) {
            config.values.push_back(*v);
        }
    }

    if (auto v = tryLoad(state, fs::path{RESOURCEPATH} / CONFIG_FILE_NAME)) {
        config.values.push_back(*v);
    }

    return config;
}

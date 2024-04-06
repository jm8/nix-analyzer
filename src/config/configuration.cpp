
#include "configuration.h"
#include <common/logging.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>
#include "common/document.h"
#include "eval.hh"
#include "util.hh"
#include "value.hh"

namespace fs = std::filesystem;

const std::string CONFIG_FILE_NAME = "nix-analyzer-config.nix";

std::optional<nix::Value*> tryLoad(nix::EvalState& state, fs::path path) {
    if (!fs::exists(path))
        return {};
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
    } else {
        std::cerr << "Failed to load default config\n";
    }

    return config;
}

// Return the base path of the match.
// Example:
// if match is 'nixpkgs/pkgs/misc/*'
// and path is /nix/store/whatever/nixpkgs/pkgs/misc/a.nix
// then base_path is /nix/store/whatever
std::optional<fs::path> check_match(std::string_view match, const std::vector<std::string> &path_components) {
    auto math_as_path = fs::path{match};
    std::vector<std::string> match_components;
    for (auto &component : math_as_path) {
        match_components.push_back(component);
    }

    if (match_components.size() == 0) {
        return {};
    }

    if (match_components[0] == "/") {
        std::cerr << "Match path must not start with '/': " << match << "\n";
        return {};
    }

    if (match_components[0] == ".") {
        std::cerr << "Match path must not start with '.': " << match << "\n";
        return {};
    }

    if (match_components.size() > path_components.size()) {
        return {};
    }

    auto j = path_components.size()-match_components.size();
    for (int i = 0; i < match_components.size(); i++) {
        if (match_components[i] != "*" && path_components[i+j] != match_components[i]) {
            return {};
        }
    }
    std::cerr << "Match found (using " << match << ")\n";

    fs::path base_path;
    for (auto i = 0; i < j; i++) {
        base_path /= path_components[i];
    }

    return {base_path};


    // // auto match_components = s
    // int n = match.
}

Ftype Config::get_ftype(nix::EvalState& state, std::string_view path) {
    std::cerr << "Getting ftype for " << path << "\n";
    std::vector<std::string> components;
    for (auto &component : fs::path{path}) {
        components.push_back(component);
    }
    auto sMatch = state.symbols.create("match");
    for (auto &f : values) {
        try {
            state.forceList(*f, nix::noPos);
        } catch (nix::Error &e) {
            REPORT_ERROR(e);
            continue;
        }
        for (auto &v : f->listItems()) {
            try {
                state.forceAttrs(*v, nix::noPos);
            } catch (nix::Error &e) {
                REPORT_ERROR(e);
                continue;
            }
            auto matchAttr = v->attrs->get(sMatch);
            if (!matchAttr) {
                return ftypeFromValue(state, v);
            }
            try {
                state.forceString(*matchAttr->value, nix::noPos);
            } catch (nix::Error &e) {
                REPORT_ERROR(e);
                continue;
            }
            auto match = std::string_view{matchAttr->value->string.s};
            if (auto base_path = check_match(match, components)) {
                std::cerr << "base_path: " << *base_path << "\n";
            }
        }
    }
    return {};
}


Ftype ftypeFromValue(nix::EvalState& state, nix::Value *v) {
    try {
        state.forceAttrs(*v, nix::noPos);
    } catch (nix::Error &e) {
        REPORT_ERROR(e);
        return {};
    }
    Ftype result;
    auto sSchema = state.symbols.create("schema");
    for (auto attr : *v->attrs) {
        if (attr.name == sSchema) {
            result.schema = {attr.value};
        }    
    }
    return result;
}

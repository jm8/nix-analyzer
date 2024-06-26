#pragma once
#include "na_config.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "position.h"
#include "config/configuration.h"

struct ContentChange {
    std::optional<Range> range;
    std::string text;
};

inline void from_json(
    const nlohmann::json& json,
    ContentChange& contentChange
) {
    auto it = json.find("range");
    if (it != json.end()) {
        contentChange.range = it->get<Range>();
    }
    json.at("text").get_to(contentChange.text);
}

struct Ftype {
    std::optional<nix::Value*> arg;
    std::optional<nix::Value*> schema;
};

// FileInfo has state that is stored for a file across multiple requests
struct FileInfo {
    std::optional<nix::Value*> flakeInputs;

    std::optional<Ftype> ftype;

    std::optional<Config> config;
};

struct Document {
    std::string uri;
    std::string source;
    std::string path;
    std::string basePath;

    FileInfo fileInfo;

    void applyContentChange(ContentChange contentChange);
};

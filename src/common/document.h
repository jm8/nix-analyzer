#include "config.h"
#include <nlohmann/json.hpp>
#include <optional>
#include "position.h"

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

struct Document {
    std::string source;
    std::string path;
    std::string basePath;

    void applyContentChange(ContentChange contentChange);
};
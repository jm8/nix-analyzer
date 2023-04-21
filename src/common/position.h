#pragma once
#include "config.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/parser-tab.hh>
#include <nix/pos.hh>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <ostream>
#include <string>

// Position in a document (0-indexed line and col)
struct Position {
    uint32_t line;
    uint32_t character;

    Position(uint32_t line, uint32_t col);
    Position(nix::Pos pos);
    Position() = default;

    nix::Pos nixPos(std::string path = {});
    nix::PosIdx posIdx(nix::EvalState& state, std::string path);

    auto operator<=>(const Position& other) const = default;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, line, character);

std::ostream& operator<<(std::ostream& os, const Position& position);

// Range in a document (end exclusive)
struct Range {
    Position start;
    Position end;

    Range(Position start, Position end);
    Range() = default;

    Range extended();

    bool contains(Position position) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Range, start, end);

std::ostream& operator<<(std::ostream& os, const Range& range);

struct Location {
    std::string uri;
    Range range;

    Location(std::string uri, Range range);
    Location(std::string uri, Position pos);
    Location(nix::Pos nixPos);
    explicit Location(nix::EvalState& state, nix::Expr* expr);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Location, uri, range);

std::ostream& operator<<(std::ostream& os, const Location& location);
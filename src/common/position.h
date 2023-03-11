#pragma once
#include "config.h"
#include <nix/error.hh>
#include <nix/parser-tab.hh>
#include <nix/pos.hh>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>

// Position in a document (0-indexed line and col)
struct Position {
    uint32_t line;
    uint32_t col;

    Position(uint32_t line, uint32_t col);
    Position(nix::Pos pos);

    nix::Pos nixPos(std::string path = {});

    auto operator<=>(const Position& other) const = default;
};

std::ostream& operator<<(std::ostream& os, const Position& position);

// Range in a document (end exclusive)
struct Range {
    Position start;
    Position end;

    Range(Position start, Position end);

    bool contains(Position position) const;
};

std::ostream& operator<<(std::ostream& os, const Range& range);
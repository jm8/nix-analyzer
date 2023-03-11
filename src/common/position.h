#pragma once
#include "config.h"
#include <nix/error.hh>
#include <nix/parser-tab.hh>
#include <nix/pos.hh>
#include <cstddef>
#include <string>

struct Position {
    uint32_t line;
    uint32_t col;

    Position(size_t line, size_t col);
    Position(nix::Pos pos);

    nix::Pos nixPos(std::string path = {});
};

struct Range {
    Position start;
    Position end;

    Range(Position start, Position end);
    Range(const YYLTYPE& loc);
};

#include "position.h"
#include <nix/parser-tab.hh>

Position::Position(uint32_t line, uint32_t col) : line(line), character(col) {}
Position::Position(nix::Pos pos)
    : line(pos.line - 1), character(pos.column - 1) {}

nix::Pos Position::nixPos(std::string path) {
    return {path, nix::foFile, line + 1, character + 1};
}

std::ostream& operator<<(std::ostream& os, const Position& position) {
    os << position.line << ":" << position.character;
    return os;
}

Range::Range(Position start, Position end) : start(start), end(end) {}

bool Range::contains(Position position) const {
    return (start <= position) && (position < end);
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
    os << range.start << "-" << range.end;
    return os;
}

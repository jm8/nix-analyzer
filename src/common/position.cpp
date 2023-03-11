#include "position.h"
#include <nix/parser-tab.hh>

Position::Position(size_t line, size_t col) : line(line), col(col) {}
Position::Position(nix::Pos pos) : line(pos.line - 1), col(pos.column - 1) {}

nix::Pos Position::nixPos(std::string path) {
    return {path, nix::foFile, line + 1, col + 1};
}

Range::Range(Position start, Position end) : start(start), end(end) {}
Range::Range(const YYLTYPE& loc)
    : start(loc.first_line - 1, loc.first_column - 1),
      end(loc.last_line - 1, loc.last_column - 1) {}

bool Range::contains(Position position) const {
    return (start <= position) && (position <= end);
}
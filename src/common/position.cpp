#include "position.h"
#include <nix/error.hh>
#include <nix/eval.hh>
#include <nix/parser-tab.hh>
#include <iostream>

Position::Position(uint32_t line, uint32_t col) : line(line), character(col) {}
Position::Position(nix::Pos pos)
    : line(pos.line - 1), character(pos.column - 1) {}

nix::Pos Position::nixPos(std::string path) {
    return {path, nix::foFile, line + 1, character + 1};
}

nix::PosIdx Position::posIdx(nix::EvalState& state, std::string path) {
    return state.positions.add({path, nix::foFile}, line + 1, character + 1);
}

std::ostream& operator<<(std::ostream& os, const Position& position) {
    os << position.line << ":" << position.character;
    return os;
}

Range::Range(Position start, Position end) : start(start), end(end) {}

bool Range::contains(Position position) const {
    return (start <= position) && (position < end);
}

Range Range::extended() {
    return {start, {end.line, end.character + 1}};
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
    os << range.start << "-" << range.end;
    return os;
}

Location::Location(std::string uri, Range range) : uri(uri), range(range) {}
Location::Location(std::string uri, Position pos) : uri(uri), range(pos, pos) {}
Location::Location(nix::Pos nixPos)
    : uri("file://" + nixPos.file), range(nixPos, nixPos) {}

Location::Location(nix::EvalState& state, nix::Expr* expr)
    : uri("file://" + state.positions[expr->startPos].file),
      range(state.positions[expr->startPos], state.positions[expr->endPos]) {}

std::ostream& operator<<(std::ostream& os, const Location& location) {
    os << location.uri << ":" << location.range;
    return os;
}
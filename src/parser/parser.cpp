#include "parser.h"
#include <nix/eval.hh>

int poscmp(nix::Pos a, nix::Pos b) {
    if (a.line > b.line) {
        return 1;
    }
    if (a.line < b.line) {
        return -1;
    }
    if (a.column > b.column) {
        return 1;
    }
    if (a.column < b.column) {
        return -1;
    }
    return 0;
}

ParseResult parse(nix::EvalState& state,
                  std::string source,
                  nix::Path path,
                  nix::Path basePath,
                  nix::Pos targetPos) {
    ParseResult analysis;
    state.parseWithCallback(
        source, path.empty() ? nix::foString : nix::foFile, path, basePath,
        state.staticBaseEnv,
        [&](auto x, nix::Pos start, nix::Pos end) {
            // fix
            // a.[cursor]b.c
            if (holds_alternative<nix::CallbackAttrPath>(x)) {
                start.column -= 1;
            }
            if (start.origin != targetPos.origin ||
                start.file != targetPos.file) {
                return;
            }

            if (!(poscmp(start, targetPos) <= 0 &&
                  poscmp(targetPos, end) <= 0)) {
                return;
            }

            if (holds_alternative<nix::Expr*>(x)) {
                auto e = get<nix::Expr*>(x);
                analysis.exprPath.push_back(e);
            } else if (holds_alternative<nix::CallbackAttrPath>(x)) {
                auto callbackAttrPath = get<nix::CallbackAttrPath>(x);
                analysis.attr = {callbackAttrPath.index,
                                 callbackAttrPath.attrPath};
            } else if (holds_alternative<nix::CallbackFormal>(x)) {
                analysis.formal = get<nix::CallbackFormal>(x).formal;
            } else if (holds_alternative<nix::CallbackInherit>(x)) {
                analysis.inherit = {get<nix::CallbackInherit>(x).expr};
            }
        },
        [&analysis](nix::ParseError error) {
            if (!error.info().errPos)
                return;
            auto errpos = *error.info().errPos;
            auto pos = nix::Pos{errpos.file, errpos.origin,
                                static_cast<uint32_t>(errpos.line),
                                static_cast<uint32_t>(errpos.column)};
            auto endpos = pos;
            endpos.column += 5;
            analysis.parseErrors.push_back(
                {error.info().msg.str(), pos, endpos});
        });
    return analysis;
}
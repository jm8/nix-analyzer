#include "config.h"
#include <nix/eval.hh>
#include <nix/nixexpr.hh>
#include <nix/util.hh>
#include <sstream>

const char* exprTypeName(nix::Expr* e) {
    if (dynamic_cast<nix::ExprInt*>(e)) {
        return "ExprInt";
    }
    if (dynamic_cast<nix::ExprFloat*>(e)) {
        return "ExprFloat";
    }
    if (dynamic_cast<nix::ExprString*>(e)) {
        return "ExprString";
    }
    if (dynamic_cast<nix::ExprPath*>(e)) {
        return "ExprPath";
    }
    if (dynamic_cast<nix::ExprAttrs*>(e)) {
        return "ExprAttrs";
    }
    if (dynamic_cast<nix::ExprVar*>(e)) {
        return "ExprVar";
    }
    if (dynamic_cast<nix::ExprLet*>(e)) {
        return "ExprLet";
    }
    if (dynamic_cast<nix::ExprList*>(e)) {
        return "ExprList";
    }
    if (dynamic_cast<nix::ExprSelect*>(e)) {
        return "ExprSelect";
    }
    if (dynamic_cast<nix::ExprOpHasAttr*>(e)) {
        return "ExprOpHasAttr";
    }
    if (dynamic_cast<nix::ExprLambda*>(e)) {
        return "ExprLambda";
    }
    if (dynamic_cast<nix::ExprCall*>(e)) {
        return "ExprCall";
    }
    if (dynamic_cast<nix::ExprWith*>(e)) {
        return "ExprWith";
    }
    if (dynamic_cast<nix::ExprIf*>(e)) {
        return "ExprIf";
    }
    if (dynamic_cast<nix::ExprAssert*>(e)) {
        return "ExprAssert";
    }
    if (dynamic_cast<nix::ExprOpNot*>(e)) {
        return "ExprOpNot";
    }
    if (dynamic_cast<nix::ExprOpEq*>(e)) {
        return "ExprOpEq";
    }
    if (dynamic_cast<nix::ExprOpNEq*>(e)) {
        return "ExprOpNEq";
    }
    if (dynamic_cast<nix::ExprOpAnd*>(e)) {
        return "ExprOpAnd";
    }
    if (dynamic_cast<nix::ExprOpOr*>(e)) {
        return "ExprOpOr";
    }
    if (dynamic_cast<nix::ExprOpImpl*>(e)) {
        return "ExprOpImpl";
    }
    if (dynamic_cast<nix::ExprOpUpdate*>(e)) {
        return "ExprOpUpdate";
    }
    if (dynamic_cast<nix::ExprOpConcatLists*>(e)) {
        return "ExprOpConcatLists";
    }
    if (dynamic_cast<nix::ExprConcatStrings*>(e)) {
        return "ExprConcatStrings";
    }
    if (dynamic_cast<nix::ExprPos*>(e)) {
        return "ExprPos";
    }
    return "???";
}

std::string stringify(nix::EvalState& state, nix::Expr* e) {
    std::stringstream ss;
    e->show(state.symbols, ss);
    return ss.str();
}

std::string stringify(nix::EvalState& state, nix::Value* v) {
    std::stringstream ss;
    v->print(state.symbols, ss);
    return ss.str();
}

std::string stringify(nix::Error& e) {
    return nix::filterANSIEscapes(e.info().msg.str(), true);
}
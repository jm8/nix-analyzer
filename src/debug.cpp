#include "debug.h"
#include "eval.hh"

using namespace std;
using namespace nix;

const char *exprTypeName(nix::Expr *e) {
    if (dynamic_cast<ExprInt *>(e)) {
        return "ExprInt";
    }
    if (dynamic_cast<ExprFloat *>(e)) {
        return "ExprFloat";
    }
    if (dynamic_cast<ExprString *>(e)) {
        return "ExprString";
    }
    if (dynamic_cast<ExprPath *>(e)) {
        return "ExprPath";
    }
    if (dynamic_cast<ExprAttrs *>(e)) {
        return "ExprAttrs";
    }
    if (dynamic_cast<ExprVar *>(e)) {
        return "ExprVar";
    }
    if (dynamic_cast<ExprLet *>(e)) {
        return "ExprLet";
    }
    if (dynamic_cast<ExprList *>(e)) {
        return "ExprList";
    }
    if (dynamic_cast<ExprSelect *>(e)) {
        return "ExprSelect";
    }
    if (dynamic_cast<ExprOpHasAttr *>(e)) {
        return "ExprOpHasAttr";
    }
    if (dynamic_cast<ExprLambda *>(e)) {
        return "ExprLambda";
    }
    if (dynamic_cast<ExprCall *>(e)) {
        return "ExprCall";
    }
    if (dynamic_cast<ExprWith *>(e)) {
        return "ExprWith";
    }
    if (dynamic_cast<ExprIf *>(e)) {
        return "ExprIf";
    }
    if (dynamic_cast<ExprAssert *>(e)) {
        return "ExprAssert";
    }
    if (dynamic_cast<ExprOpNot *>(e)) {
        return "ExprOpNot";
    }
    if (dynamic_cast<ExprOpEq *>(e)) {
        return "ExprOpEq";
    }
    if (dynamic_cast<ExprOpNEq *>(e)) {
        return "ExprOpNEq";
    }
    if (dynamic_cast<ExprOpAnd *>(e)) {
        return "ExprOpAnd";
    }
    if (dynamic_cast<ExprOpOr *>(e)) {
        return "ExprOpOr";
    }
    if (dynamic_cast<ExprOpImpl *>(e)) {
        return "ExprOpImpl";
    }
    if (dynamic_cast<ExprOpUpdate *>(e)) {
        return "ExprOpUpdate";
    }
    if (dynamic_cast<ExprOpConcatLists *>(e)) {
        return "ExprOpConcatLists";
    }
    if (dynamic_cast<ExprConcatStrings *>(e)) {
        return "ExprConcatStrings";
    }
    if (dynamic_cast<ExprPos *>(e)) {
        return "ExprPos";
    }
    return "???";
}
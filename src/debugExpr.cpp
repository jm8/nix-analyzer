#include "debugExpr.h"
#include "eval.hh"

using namespace std;
using namespace nix;

void debugExpr(EvalState &state, std::ostream &s, nix::Expr *e, int indent) {
    s << string(indent, ' ') << state.positions[e->getPos()] << ' ';
    if (dynamic_cast<ExprInt *>(e)) {
        s << "ExprInt";
    }
    if (dynamic_cast<ExprFloat *>(e)) {
        s << "ExprFloat";
    }
    if (dynamic_cast<ExprString *>(e)) {
        s << "ExprString";
    }
    if (dynamic_cast<ExprPath *>(e)) {
        s << "ExprPath";
    }
    if (dynamic_cast<ExprAttrs *>(e)) {
        s << "ExprAttrs";
    }
    if (dynamic_cast<ExprVar *>(e)) {
        s << "ExprVar";
    }
    if (dynamic_cast<ExprLet *>(e)) {
        s << "ExprLet";
    }
    if (dynamic_cast<ExprList *>(e)) {
        s << "ExprList";
    }
    if (dynamic_cast<ExprSelect *>(e)) {
        s << "ExprSelect";
    }
    if (dynamic_cast<ExprOpHasAttr *>(e)) {
        s << "ExprOpHasAttr";
    }
    if (dynamic_cast<ExprLambda *>(e)) {
        s << "ExprLambda";
    }
    if (dynamic_cast<ExprCall *>(e)) {
        s << "ExprCall";
    }
    if (dynamic_cast<ExprWith *>(e)) {
        s << "ExprWith";
    }
    if (dynamic_cast<ExprIf *>(e)) {
        s << "ExprIf";
    }
    if (dynamic_cast<ExprAssert *>(e)) {
        s << "ExprAssert";
    }
    if (dynamic_cast<ExprOpNot *>(e)) {
        s << "ExprOpNot";
    }
    if (dynamic_cast<ExprOpEq *>(e)) {
        s << "ExprOpEq";
    }
    if (dynamic_cast<ExprOpNEq *>(e)) {
        s << "ExprOpNEq";
    }
    if (dynamic_cast<ExprOpAnd *>(e)) {
        s << "ExprOpAnd";
    }
    if (dynamic_cast<ExprOpOr *>(e)) {
        s << "ExprOpOr";
    }
    if (dynamic_cast<ExprOpImpl *>(e)) {
        s << "ExprOpImpl";
    }
    if (dynamic_cast<ExprOpUpdate *>(e)) {
        s << "ExprOpUpdate";
    }
    if (dynamic_cast<ExprOpConcatLists *>(e)) {
        s << "ExprOpConcatLists";
    }
    if (dynamic_cast<ExprConcatStrings *>(e)) {
        s << "ExprConcatStrings";
    }
    if (dynamic_cast<ExprPos *>(e)) {
        s << "ExprPos";
    }
    s << endl;
}
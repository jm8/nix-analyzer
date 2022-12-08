#include "nix-analyzer.h"

using namespace std;
using namespace nix;

void NixAnalyzer::debugExpr(std::ostream &s, nix::Expr *e, int indent) {
    s << string(indent, ' ') << state->positions[e->getPos()] << ' ';
    if (ExprInt *e2 = dynamic_cast<ExprInt *>(e)) {
        s << "ExprInt";
    }
    if (ExprFloat *e2 = dynamic_cast<ExprFloat *>(e)) {
        s << "ExprFloat";
    }
    if (ExprString *e2 = dynamic_cast<ExprString *>(e)) {
        s << "ExprString";
    }
    if (ExprPath *e2 = dynamic_cast<ExprPath *>(e)) {
        s << "ExprPath";
    }
    if (ExprAttrs *e2 = dynamic_cast<ExprAttrs *>(e)) {
        s << "ExprAttrs";
    }
    if (ExprVar *e2 = dynamic_cast<ExprVar *>(e)) {
        s << "ExprVar";
    }
    if (ExprLet *e2 = dynamic_cast<ExprLet *>(e)) {
        s << "ExprLet";
    }
    if (ExprList *e2 = dynamic_cast<ExprList *>(e)) {
        s << "ExprList";
    }
    if (ExprSelect *e2 = dynamic_cast<ExprSelect *>(e)) {
        s << "ExprSelect";
    }
    if (ExprOpHasAttr *e2 = dynamic_cast<ExprOpHasAttr *>(e)) {
        s << "ExprOpHasAttr";
    }
    if (ExprLambda *e2 = dynamic_cast<ExprLambda *>(e)) {
        s << "ExprLambda";
    }
    if (ExprCall *e2 = dynamic_cast<ExprCall *>(e)) {
        s << "ExprCall"
          << "\n";
        debugExpr(s, e2->fun, indent + 2);
        for (Expr *arg : e2->args) {
            debugExpr(s, arg, indent + 2);
        }
    }
    if (ExprWith *e2 = dynamic_cast<ExprWith *>(e)) {
        s << "ExprWith";
    }
    if (ExprIf *e2 = dynamic_cast<ExprIf *>(e)) {
        s << "ExprIf";
    }
    if (ExprAssert *e2 = dynamic_cast<ExprAssert *>(e)) {
        s << "ExprAssert";
    }
    if (ExprOpNot *e2 = dynamic_cast<ExprOpNot *>(e)) {
        s << "ExprOpNot";
    }
    if (ExprOpEq *e2 = dynamic_cast<ExprOpEq *>(e)) {
        s << "ExprOpEq";
    }
    if (ExprOpNEq *e2 = dynamic_cast<ExprOpNEq *>(e)) {
        s << "ExprOpNEq";
    }
    if (ExprOpAnd *e2 = dynamic_cast<ExprOpAnd *>(e)) {
        s << "ExprOpAnd";
    }
    if (ExprOpOr *e2 = dynamic_cast<ExprOpOr *>(e)) {
        s << "ExprOpOr";
    }
    if (ExprOpImpl *e2 = dynamic_cast<ExprOpImpl *>(e)) {
        s << "ExprOpImpl";
    }
    if (ExprOpUpdate *e2 = dynamic_cast<ExprOpUpdate *>(e)) {
        s << "ExprOpUpdate";
    }
    if (ExprOpConcatLists *e2 = dynamic_cast<ExprOpConcatLists *>(e)) {
        s << string(indent, ' ') << "ExprOpConcatLists";
    }
    if (ExprConcatStrings *e2 = dynamic_cast<ExprConcatStrings *>(e)) {
        s << string(indent, ' ') << "ExprConcatStrings";
    }
    if (ExprPos *e2 = dynamic_cast<ExprPos *>(e)) {
        s << string(indent, ' ') << "ExprPos";
    }
    s << endl;
}
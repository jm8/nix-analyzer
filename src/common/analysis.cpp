#include "analysis.h"
#include <nix/nixexpr.hh>

ExprPathItem::ExprPathItem(nix::Expr* e) : e(e) {}

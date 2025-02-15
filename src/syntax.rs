use std::iter;

use itertools::Itertools;
use rnix::{
    ast::{Attr, Attrpath, Expr, HasEntry, Inherit, Param, PatBind, PatEntry, Root},
    SyntaxKind, SyntaxNode, SyntaxToken, TextSize,
};
use rowan::ast::AstNode;
use tracing::info;

use crate::{
    lambda_arg::get_lambda_arg,
    safe_stringification::{
        safe_stringify, safe_stringify_attr, safe_stringify_bindings, safe_stringify_opt_param,
        safe_stringify_pattern,
    },
    FileType,
};

const BUILTIN_IDS: [&'static str; 23] = [
    "abort",
    "baseNameOf",
    "break",
    "builtins",
    "derivation",
    "derivationStrict",
    "dirOf",
    "false",
    "fetchGit",
    "fetchMercurial",
    "fetchTarball",
    "fetchTree",
    "fromTOML",
    "import",
    "isNull",
    "map",
    "null",
    "placeholder",
    "removeAttrs",
    "scopedImport",
    "throw",
    "toString",
    "true",
];

pub fn parse(source: &str) -> Root {
    rnix::Root::parse(source).tree()
}

#[derive(Debug)]
pub enum LocationWithinExpr {
    Normal,
    Inherit(Inherit),
    Attrpath(Attrpath, usize),
    PatEntry,
    PatBind,
}

#[derive(Debug)]
pub struct TokenLocation {
    pub token: SyntaxToken,
    pub expr: Expr,
    pub location_within: LocationWithinExpr,
}

// We might be on whitespace or some trivial node.
// Find the nearest actually useful node.
pub fn nearest_expr(node: SyntaxNode) -> Expr {
    let mut node = node;
    while !Expr::can_cast(node.kind()) {
        // It's okay to unwrap since a Root expr always exists at the top.
        node = node.parent().unwrap();
    }
    Expr::cast(node).unwrap()
}

pub fn ancestor_exprs(expr: &Expr) -> impl Iterator<Item = Expr> {
    let mut result = vec![];
    let mut curr_node = expr.syntax().parent();
    while let Some(node) = curr_node
        && node.kind() != SyntaxKind::NODE_ROOT
    {
        let expr = nearest_expr(node);
        result.push(expr.clone());
        curr_node = expr.syntax().parent();
    }
    result.into_iter()
}

pub fn locate_cursor(root: &Root, offset: u32) -> Option<TokenLocation> {
    let token = root
        .syntax()
        .token_at_offset(TextSize::from(offset))
        .into_iter()
        .max_by_key(|token| match token.kind() {
            SyntaxKind::TOKEN_IDENT => 2,
            SyntaxKind::TOKEN_WHITESPACE => 0,
            _ => 1,
        })?;

    locate_token(&token)
}

fn locate_token(token: &SyntaxToken) -> Option<TokenLocation> {
    let mut expr = nearest_expr(token.parent().unwrap());
    let mut location_within = LocationWithinExpr::Normal;
    // This Ident node could be part of an attrpath, inherit, or formals.
    if let Some(attrpath) = Attrpath::cast(expr.syntax().parent().unwrap()) {
        let (pos, _attr) = attrpath
            .attrs()
            .find_position(|attr| attr.syntax() == expr.syntax())
            .unwrap();
        location_within = LocationWithinExpr::Attrpath(attrpath.clone(), pos);
        expr = nearest_expr(attrpath.syntax().clone());
    } else if let Some(inherit) = Inherit::cast(expr.syntax().parent().unwrap()) {
        location_within = LocationWithinExpr::Inherit(inherit.clone());
        expr = nearest_expr(inherit.syntax().clone());
    } else if let Some(pat_entry) = PatEntry::cast(expr.syntax().parent().unwrap()) {
        location_within = LocationWithinExpr::PatEntry;
        expr = nearest_expr(pat_entry.syntax().clone());
    } else if let Some(pat_bind) = PatBind::cast(expr.syntax().parent().unwrap()) {
        location_within = LocationWithinExpr::PatBind;
        expr = nearest_expr(pat_bind.syntax().clone());
    }

    Some(TokenLocation {
        expr,
        location_within,
        token: token.clone(),
    })
}

pub fn in_context_with_select(
    expr: &Expr,
    attrs: impl Iterator<Item = Attr>,
    file_type: &FileType,
) -> String {
    let mut string_to_eval = safe_stringify(&expr);
    for attr in attrs {
        string_to_eval = format!("{}.{}", string_to_eval, safe_stringify_attr(&attr));
    }

    for ancestor in ancestor_exprs(&expr) {
        match ancestor {
            Expr::LetIn(ref letin) => {
                string_to_eval = format!(
                    "(let {} in ({}))",
                    safe_stringify_bindings(letin),
                    string_to_eval
                );
            }
            Expr::Lambda(ref lambda) => {
                let arg = get_lambda_arg(&lambda, file_type);
                string_to_eval = format!(
                    "(({}: {}) {})",
                    safe_stringify_opt_param(lambda.param().as_ref()),
                    string_to_eval,
                    arg
                );
            }
            _ => continue,
        }
    }

    info!(?string_to_eval);

    string_to_eval
}

pub fn in_context(expr: &Expr, file_type: &FileType) -> String {
    in_context_with_select(expr, iter::empty(), file_type)
}

pub fn with_expression(expr: &Expr, file_type: &FileType) -> Option<String> {
    let with = ancestor_exprs(&expr).find_map(|ancestor| match ancestor {
        Expr::With(with) => Some(with),
        _ => None,
    })?;

    Some(in_context(&with.namespace()?, file_type))
}

pub fn get_variables(expr: &Expr) -> Vec<String> {
    let mut variables = vec![];

    fn add_from_entires(variables: &mut Vec<String>, entries: impl HasEntry) {
        let attr_to_string = |attr| match attr {
            Attr::Ident(ident) => Some(ident.to_string()),
            Attr::Dynamic(_) => None,
            Attr::Str(_) => None, // TODO: this should handle valid identifiers: let "hello" = null; in $0
        };

        for entry in entries.attrpath_values() {
            let var = entry
                .attrpath()
                .and_then(|attrpath| attrpath.attrs().nth(0))
                .and_then(attr_to_string);

            if let Some(var) = var {
                variables.push(var);
            }
        }
        for inherit in entries.inherits() {
            for attr in inherit.attrs() {
                let var = attr_to_string(attr);
                if let Some(var) = var {
                    variables.push(var);
                }
            }
        }
    }

    for ancestor in ancestor_exprs(&expr) {
        match ancestor {
            Expr::Lambda(lambda) => match lambda.param() {
                Some(Param::Pattern(pattern)) => {
                    if let Some(ident) = pattern.pat_bind().and_then(|bind| bind.ident()) {
                        variables.push(ident.to_string());
                    }
                    for entry in pattern.pat_entries() {
                        if let Some(ident) = entry.ident() {
                            variables.push(ident.to_string());
                        }
                    }
                }
                Some(Param::IdentParam(ident_param)) => {
                    variables.push(ident_param.to_string());
                }
                None => {}
            },
            Expr::LegacyLet(legacy_let) => add_from_entires(&mut variables, legacy_let),
            Expr::LetIn(let_in) => add_from_entires(&mut variables, let_in),
            Expr::AttrSet(attr_set) => {
                if attr_set.rec_token().is_some() {
                    add_from_entires(&mut variables, attr_set)
                }
            }
            _ => {}
        }
    }

    for var in BUILTIN_IDS {
        variables.push(var.to_string());
    }

    variables
}

// #[cfg(test)]
// mod test {
//     use expect_test::{expect, Expect};
//     use itertools::Itertools;
//     use rnix::TextSize;
//     use rowan::ast::AstNode;

//     use crate::evaluator::Evaluator;

//     use super::{in_context, locate_token, parse};

//     async fn check_eval_in_context(source: &str, expected: Expect) {
//         let (left, right) = source.split("$0").collect_tuple().unwrap();
//         let offset = TextSize::new(left.len() as u32);

//         let source = format!("{}{}", left, right);
//         let root = parse(&source);
//         let token = root
//             .syntax()
//             .token_at_offset(offset)
//             .max_by_key(|token| match token.kind() {
//                 rnix::SyntaxKind::TOKEN_IDENT => 2,
//                 rnix::SyntaxKind::TOKEN_WHITESPACE => 0,
//                 _ => 1,
//             })
//             .unwrap();

//         let location = locate_token(&token).unwrap();
//         let expression = in_context(&location.expr);

//         let evaluator = Evaluator::new();

//         expected.assert_eq(
//             &evaluator
//                 .get_attributes(&crate::evaluator::GetAttributesRequest { expression })
//                 .await
//                 .unwrap(),
//         );
//     }

//     #[test]
//     fn test_eval_in_context_let_in() {
//         check_eval_in_context(r#"let x = 1234; in x$0"#, expect!["1234"]);
//     }

//     #[test]
//     fn test_eval_nixpkgs_1() {
//         check_eval_in_context(
//             r#"(((import /nix/store/y8pbnccb8bc1p8fchx7zkb0874yblrg3-source)  {  })).hello.version$0"#,
//             expect![[r#""2.12.1""#]],
//         );
//     }
// }

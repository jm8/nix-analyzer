use itertools::Itertools;
use lazy_regex::regex;
use lsp_types::{Position, Range};
use rnix::{
    ast::{
        Attr, Attrpath, AttrpathValue, Expr, HasEntry, IdentParam, Inherit, Param, PatBind,
        PatEntry, Root,
    },
    SyntaxKind, SyntaxNode, SyntaxToken, TextRange, TextSize,
};
use ropey::Rope;
use rowan::ast::AstNode;
use std::{fmt, iter};
use tracing::info;

use crate::{
    file_types::FileInfo,
    lambda_arg::get_lambda_arg,
    safe_stringification::{
        safe_stringify, safe_stringify_attr, safe_stringify_bindings, safe_stringify_opt,
        safe_stringify_opt_param,
    },
};

const BUILTIN_IDS: [&str; 23] = [
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
    Inherit(Inherit, usize),
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

pub fn ancestor_exprs_inclusive(expr: &Expr) -> impl Iterator<Item = Expr> {
    std::iter::once(expr.clone()).chain(ancestor_exprs(expr))
}

pub fn locate_cursor(root: &Root, offset: u32) -> Option<TokenLocation> {
    let token = root
        .syntax()
        .token_at_offset(TextSize::from(offset))
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
        let (pos, _attr) = inherit
            .attrs()
            .find_position(|attr| attr.syntax() == expr.syntax())
            .unwrap();
        location_within = LocationWithinExpr::Inherit(inherit.clone(), pos);
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

pub fn in_context_custom(
    string_to_eval: &str,
    ancestors: impl Iterator<Item = Expr>,
    file_info: &FileInfo,
) -> String {
    let mut string_to_eval = string_to_eval.to_string();
    for ancestor in ancestors {
        match ancestor {
            Expr::LetIn(ref letin) => {
                string_to_eval = format!(
                    "(let {} in ({}))",
                    safe_stringify_bindings(letin, file_info.base_path()),
                    string_to_eval
                );
            }
            Expr::AttrSet(ref attr_set) => {
                if attr_set.rec_token().is_some() {
                    string_to_eval = format!(
                        "(let {} in ({}))",
                        safe_stringify_bindings(attr_set, file_info.base_path()),
                        string_to_eval
                    );
                }
            }
            Expr::Lambda(ref lambda) => {
                let arg = get_lambda_arg(lambda, file_info);
                string_to_eval = format!(
                    "(({}: {}) {})",
                    safe_stringify_opt_param(lambda.param().as_ref(), file_info.base_path()),
                    string_to_eval,
                    arg
                );
            }
            Expr::With(ref with) => {
                string_to_eval = format!(
                    "(with {}; {})",
                    safe_stringify_opt(with.namespace().as_ref(), file_info.base_path()),
                    string_to_eval,
                );
            }

            _ => continue,
        }
    }

    info!(?string_to_eval);

    string_to_eval
}

pub fn in_context_with_select(
    expr: &Expr,
    attrs: impl Iterator<Item = Attr>,
    file_info: &FileInfo,
) -> String {
    let mut string_to_eval = safe_stringify(expr, file_info.base_path());
    for attr in attrs {
        string_to_eval = format!(
            "{}.{}",
            string_to_eval,
            safe_stringify_attr(&attr, file_info.base_path())
        );
    }

    in_context_custom(&string_to_eval, ancestor_exprs_inclusive(expr), file_info)
}

pub fn in_context(expr: &Expr, file_info: &FileInfo) -> String {
    in_context_with_select(expr, iter::empty(), file_info)
}

pub fn with_expression(expr: &Expr, file_info: &FileInfo) -> Option<String> {
    let with = ancestor_exprs(expr).find_map(|ancestor| match ancestor {
        Expr::With(with) => Some(with),
        _ => None,
    })?;

    Some(in_context(&with.namespace()?, file_info))
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
                .and_then(|attrpath| attrpath.attrs().next())
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

    for ancestor in ancestor_exprs(expr) {
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

pub enum FoundVariable {
    PatBind(PatBind),
    PatEntry(PatEntry),
    IdentParam(IdentParam),
    AttrpathValue(AttrpathValue),
    Inherit(Inherit),
    Builtin,
}

pub fn find_variable(expr: &Expr, name: &str) -> Option<FoundVariable> {
    fn search_in_entries(entries: impl HasEntry, name: &str) -> Option<FoundVariable> {
        let attr_to_string = |attr| match attr {
            Attr::Ident(ident) => Some(ident.to_string()),
            Attr::Dynamic(_) => None,
            Attr::Str(_) => None, // TODO: this should handle valid identifiers: let "hello" = null; in $0
        };

        for entry in entries.attrpath_values() {
            let var = entry
                .attrpath()
                .and_then(|attrpath| attrpath.attrs().next())
                .and_then(attr_to_string);

            if var.as_ref().map(|s| s.as_str()) == Some(name) {
                return Some(FoundVariable::AttrpathValue(entry));
            }
        }
        for inherit in entries.inherits() {
            for attr in inherit.attrs() {
                let var = attr_to_string(attr);
                if var.as_ref().map(|s| s.as_str()) == Some(name) {
                    return Some(FoundVariable::Inherit(inherit));
                }
            }
        }
        return None;
    }

    for ancestor in ancestor_exprs(expr) {
        match ancestor {
            Expr::Lambda(lambda) => match lambda.param() {
                Some(Param::Pattern(pattern)) => {
                    if let Some(pat_bind) = pattern.pat_bind()
                        && let Some(ident) = pat_bind.ident()
                        && ident.to_string().as_str() == name
                    {
                        return Some(FoundVariable::PatBind(pat_bind));
                    }
                    for entry in pattern.pat_entries() {
                        if let Some(ident) = entry.ident()
                            && ident.to_string().as_str() == name
                        {
                            return Some(FoundVariable::PatEntry(entry));
                        }
                    }
                }
                Some(Param::IdentParam(ident_param)) => {
                    if let Some(ident) = ident_param.ident()
                        && ident.to_string() == name
                    {
                        return Some(FoundVariable::IdentParam(ident_param));
                    }
                }
                None => {}
            },
            Expr::LegacyLet(legacy_let) => {
                if let Some(found) = search_in_entries(legacy_let, name) {
                    return Some(found);
                }
            }
            Expr::LetIn(let_in) => {
                if let Some(found) = search_in_entries(let_in, name) {
                    return Some(found);
                }
            }
            Expr::AttrSet(attr_set) => {
                if attr_set.rec_token().is_some() {
                    if let Some(found) = search_in_entries(attr_set, name) {
                        return Some(found);
                    }
                }
            }
            _ => {}
        }
    }

    for builtin in BUILTIN_IDS {
        if name == builtin {
            return Some(FoundVariable::Builtin);
        }
    }

    None
}

pub fn escape_attr(attr: &str) -> String {
    let re = regex!("^[A-Za-z_][A-Za-z_0-9'-]*$");
    if re.is_match(attr) {
        attr.to_string()
    } else {
        escape_string(attr)
    }
}

pub fn escape_string(text: &str) -> String {
    format!("\"{}\"", EscapeStringFragment(text))
}
#[derive(Debug, Clone)]
pub struct EscapeStringFragment<'a>(pub &'a str);

impl fmt::Display for EscapeStringFragment<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for (i, ch) in self.0.char_indices() {
            match ch {
                '"' => "\\\"",
                '\\' => "\\\\",
                '\n' => "\\n",
                '\r' => "\\r",
                '\t' => "\\r",
                '$' if self.0[i..].starts_with("${") => "\\$",
                _ => {
                    ch.fmt(f)?;
                    continue;
                }
            }
            .fmt(f)?;
        }
        Ok(())
    }
}

pub fn rope_offset_to_position(rope: &Rope, offset: impl Into<usize>) -> Position {
    let offset = Into::<usize>::into(offset);
    let line = rope.byte_to_line(offset) as u32;
    let character = (offset - rope.line_to_byte(line as usize)) as u32;
    Position { line, character }
}

pub fn rope_text_range_to_range(rope: &Rope, text_range: TextRange) -> Range {
    let start = rope_offset_to_position(rope, text_range.start());
    let end = rope_offset_to_position(rope, text_range.end());
    Range { start, end }
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

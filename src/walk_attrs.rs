use rnix::ast::{AttrSet, AttrpathValue, Expr, HasEntry, List};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Subscript {
    Attr(String),
    ListItem(usize),
    Argument,
    LambdaBody,
    LetBody,
}

#[derive(Clone, Debug)]
pub struct Item {
    pub position: Vec<Subscript>,
    pub expr: Expr,
}
pub fn walk_attrs(attrs: &AttrSet) -> Vec<Item> {
    fn rec_attrs(items: &mut Vec<Item>, stack: &[Subscript], attrs: &AttrSet) -> Option<()> {
        for attrpath_value in attrs.attrpath_values() {
            rec_attrpath_value(items, stack, &attrpath_value);
        }
        Some(())
    }

    fn rec_list(items: &mut Vec<Item>, stack: &[Subscript], list: &List) -> Option<()> {
        for (i, item) in list.items().enumerate() {
            rec_expr(
                items,
                &stack
                    .iter()
                    .cloned()
                    .chain([Subscript::ListItem(i)])
                    .collect::<Vec<_>>(),
                &item,
            );
        }
        Some(())
    }

    fn rec_attrpath_value(
        items: &mut Vec<Item>,
        stack: &[Subscript],
        attrpath_value: &AttrpathValue,
    ) -> Option<()> {
        let expr = attrpath_value.value()?;
        let attrs = attrpath_value
            .attrpath()?
            .attrs()
            .map(|attr| match attr {
                rnix::ast::Attr::Ident(ident) => Some(Subscript::Attr(ident.to_string())),
                rnix::ast::Attr::Dynamic(_) => None,
                rnix::ast::Attr::Str(str) => {
                    // TODO: make better (None if has ${})
                    Some(Subscript::Attr(str.to_string()))
                }
            })
            .try_collect::<Vec<Subscript>>()?;

        let stack = stack
            .iter()
            .cloned()
            .chain(attrs.into_iter())
            .collect::<Vec<_>>();

        rec_expr(items, &stack, &expr);

        Some(())
    }

    fn rec_expr(items: &mut Vec<Item>, stack: &[Subscript], expr: &Expr) -> Option<()> {
        items.push(Item {
            position: stack.to_vec(),
            expr: expr.clone(),
        });
        match expr {
            Expr::Apply(apply) => {
                rec_expr(
                    items,
                    &stack
                        .iter()
                        .cloned()
                        .chain([Subscript::Argument])
                        .collect::<Vec<_>>(),
                    &apply.argument()?,
                );
            }
            Expr::AttrSet(attrs) => {
                rec_attrs(items, stack, attrs);
            }
            Expr::List(list) => {
                rec_list(items, stack, list);
            }
            Expr::Lambda(lambda) => {
                rec_expr(
                    items,
                    &stack
                        .iter()
                        .cloned()
                        .chain([Subscript::LambdaBody])
                        .collect::<Vec<_>>(),
                    &lambda.body()?,
                );
            }
            Expr::LetIn(let_in) => {
                rec_expr(
                    items,
                    &stack
                        .iter()
                        .cloned()
                        .chain([Subscript::LetBody])
                        .collect::<Vec<_>>(),
                    &let_in.body()?,
                );
            }
            _ => {}
        }
        Some(())
    }

    let mut items: Vec<Item> = vec![];
    rec_attrs(&mut items, &[], attrs);
    items
}

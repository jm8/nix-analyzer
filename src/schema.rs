use std::collections::HashMap;

use itertools::Itertools;
use rnix::ast::{Attr, Expr, HasEntry};
use serde::Deserialize;
use tracing::info;

use crate::{
    syntax::{ancestor_exprs, ancestor_exprs_inclusive},
    FileType,
};

#[derive(Debug, Deserialize, Clone)]
#[serde(untagged)]
enum Value {
    String(String),
}

#[derive(Debug, Deserialize, Clone, Copy)]
#[serde(rename_all = "camelCase")]
enum SimpleType {
    Object,
    Boolean,
    String,
}

#[derive(Debug, Deserialize, Clone)]
#[serde(untagged)]
enum Type {
    Simple(SimpleType),
    Enum { r#enum: Vec<Value> },
}

#[derive(Debug, Deserialize, Default, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Schema {
    r#type: Option<Type>,
    description: Option<String>,
    additional_properties: Option<Box<Schema>>,
    properties: Option<HashMap<String, Schema>>,
}

const NULL_SCHEMA: Schema = Schema {
    r#type: None,
    description: None,
    additional_properties: None,
    properties: None,
};

impl Schema {
    pub fn properties(&self) -> Vec<String> {
        self.properties
            .as_ref()
            .map(|properties| properties.keys().cloned().collect_vec())
            .unwrap_or_default()
    }

    pub fn attr_subschema(&self, attr: &Attr) -> &Schema {
        let property = match attr {
            Attr::Ident(ident) => Some(ident.to_string()),
            Attr::Dynamic(_) => None,
            Attr::Str(_) => {
                // TODO
                None
            }
        };

        property
            .and_then(|property| {
                self.properties
                    .as_ref()
                    .and_then(|properties| properties.get(&property))
            })
            .or(self.additional_properties.as_deref())
            .unwrap_or(&NULL_SCHEMA)
    }
}

pub fn get_schema(expr: &Expr, file_type: &FileType) -> Schema {
    println!("BBBBBBBBBBB");
    let (root_schema, root_expr) = get_schema_root(expr, file_type);
    println!("CCCCCCCCC {:?}", root_schema);
    println!("DDDDDDDDDD {}", root_expr);

    let mut schema = root_schema;

    for (child, parent) in ancestor_exprs_inclusive(expr)
        .tuple_windows()
        .take_while_inclusive(|(_child, parent)| *parent != root_expr)
    {
        match parent {
            Expr::AttrSet(ref attr_set) => {
                match attr_set
                    .attrpath_values()
                    .find(|attrpath_value| attrpath_value.value().as_ref() == Some(&child))
                {
                    Some(attrpath_value) => {
                        for attr in attrpath_value
                            .attrpath()
                            .iter()
                            .flat_map(|attrpath| attrpath.attrs())
                        {
                            schema = schema.attr_subschema(&attr).clone();
                        }
                    }
                    None => return NULL_SCHEMA,
                }
            }
            _ => return NULL_SCHEMA,
        }
        println!("PARNET {}", parent);
        println!("CHILD {}", child);
    }

    // Schema::default()
    schema
}

pub fn get_schema_root(expr: &Expr, file_type: &FileType) -> (Schema, Expr) {
    let root_expr = ancestor_exprs_inclusive(expr).last().unwrap();
    let root_schema = match file_type {
        FileType::Package { nixpkgs_path: _ } => Schema::default(),
        FileType::Custom {
            lambda_arg: _,
            schema,
        } => serde_json::from_str(&schema).unwrap_or_default(),
    };
    (root_schema, root_expr)
}

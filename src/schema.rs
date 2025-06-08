use crate::{
    file_types::{FileInfo, FileType},
    syntax::ancestor_exprs_inclusive,
};
use itertools::Itertools;
use lazy_static::lazy_static;
use rnix::ast::{Attr, Expr, HasEntry};
use serde::Deserialize;
use std::{collections::HashMap, sync::Arc};

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
    additional_properties: Option<Arc<Schema>>,
    properties: Option<HashMap<String, Arc<Schema>>>,
}

lazy_static! {
    pub static ref NIXOS_MODULE_SCHEMA: Arc<Schema> =
        Arc::new(serde_json::from_str(include_str!("nixos_module_schema.json")).unwrap());
    pub static ref HOME_MANAGER_SCHEMA: Arc<Schema> =
        Arc::new(serde_json::from_str(include_str!("home_manager_schema.json")).unwrap());
    pub static ref FLAKE_SCHEMA: Arc<Schema> =
        Arc::new(serde_json::from_str(include_str!("flake_schema.json")).unwrap());
}

impl Schema {
    pub fn description(&self) -> Option<&str> {
        self.description.as_deref()
    }

    pub fn properties(&self) -> Vec<String> {
        self.properties
            .as_ref()
            .map(|properties| properties.keys().cloned().collect_vec())
            .unwrap_or_default()
    }

    pub fn attr_subschema(&self, attr: &Attr) -> Arc<Schema> {
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
            .or(self.additional_properties.as_ref())
            .cloned()
            .unwrap_or(Arc::new(Schema::default()))
    }
}

pub fn get_schema(expr: &Expr, file_info: &FileInfo) -> Arc<Schema> {
    let (root_schema, root_expr) = get_schema_root(expr, file_info);

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
                    None => return Arc::new(Schema::default()),
                }
            }
            Expr::LetIn(_) | Expr::Lambda(_) | Expr::With(_) => {}
            _ => return Arc::new(Schema::default()),
        }
    }

    schema
}

pub fn get_schema_root(expr: &Expr, file_info: &FileInfo) -> (Arc<Schema>, Expr) {
    let root_expr = ancestor_exprs_inclusive(expr).last().unwrap();
    let root_schema = match &file_info.file_type {
        FileType::Other {
            nixpkgs_path: _,
            schema,
        } => schema.clone(),
        FileType::Custom {
            lambda_arg: _,
            schema,
        } => Arc::new(serde_json::from_str(schema).unwrap_or_default()),
        FileType::Flake { .. } => FLAKE_SCHEMA.clone(),
        FileType::Nixpkgs { .. } => Arc::new(Schema::default()),
    };
    (root_schema, root_expr)
}

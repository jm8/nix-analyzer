use std::path::PathBuf;

use anyhow::{bail, Result};

pub fn hover(_source: &str, _offset: u32) -> Result<HoverResult> {
    bail!("hover")
}

pub struct Position {
    pub line: u32,
    pub col: u32,
    pub path: PathBuf,
}

pub struct HoverResult {
    pub md: String,
    pub position: Position,
}

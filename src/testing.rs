use std::{
    collections::BTreeMap,
    path::{Path, PathBuf},
};

use ropey::Rope;

use crate::syntax::rope_offset_to_position;

#[derive(Debug)]
pub struct Location {
    pub path: PathBuf,
    pub line: u32,
    pub col: u32,
}

#[derive(Debug)]
pub struct TestInput {
    pub files: BTreeMap<PathBuf, String>,
    pub location: Option<Location>,
}

pub fn parse_test_input(input: &str) -> TestInput {
    let mut files = BTreeMap::new();
    let mut lines = input.lines().peekable();
    let mut current_path = PathBuf::from("/nowhere.nix");
    let mut current_content = Vec::new();
    let mut location = None;

    let mut add_file = |current_path: &Path, current_content: &[String]| {
        if !current_content.is_empty() {
            let mut content = current_content.join("\n");
            if let Some(offset) = content.find("$0") {
                content = format!("{}{}", &content[0..offset], &content[offset + 2..]);
                let rope = Rope::from_str(&content);
                let position = rope_offset_to_position(&rope, offset);
                location = Some(Location {
                    path: current_path.to_path_buf(),
                    line: position.line,
                    col: position.character,
                })
            }
            files.insert(current_path.to_path_buf(), content);
        }
    };

    while let Some(line) = lines.next() {
        eprintln!("LINE = {:?}", line);
        if line.starts_with("## ") {
            add_file(&current_path, &current_content);
            current_content.clear();
            current_path = PathBuf::from(line[2..].trim());
        } else {
            current_content.push(line.to_string());
        }
    }

    add_file(&current_path, &current_content);

    TestInput { files, location }
}

// pub async fn create_test_analyzer(input: &TestInput) {
//     let evaluator = Evaluator::new().await;
//     let (fetcher_input_send, fetcher_input_recv) = crossbeam::channel::unbounded::<FetcherInput>();
//     let (fetcher_output_send, fetcher_output_recv) =
//         crossbeam::channel::unbounded::<FetcherOutput>();

//     let mut analyzer = Analyzer::new(evaluator, fetcher_input_send, fetcher_output_recv);

//     for (path, content) in input.files.iter() {
//         analyzer.change_file(&path, &content);
//     }

// }

#[cfg(test)]
mod test {
    use expect_test::{expect, Expect};
    use indoc::indoc;

    use super::parse_test_input;

    fn check_parse_test_input(input: &str, expected: Expect) {
        expected.assert_debug_eq(&parse_test_input(input.trim()));
    }

    #[test]
    fn test_parse_test_input() {
        check_parse_test_input(
            indoc! {r#"
            ## /test.nix
            aaaaaaaaaaaaaaa
            bbbbbb
            cccccccc
            ## /your mother.nix
            aaaaaaaaaaaaaaaaaaaazzzzzzzzzzzzz
            xyz.$0cyz
            "#},
            expect![[r#"
                TestInput {
                    files: {
                        "/test.nix": "aaaaaaaaaaaaaaa\nbbbbbb\ncccccccc",
                        "/your mother.nix": "aaaaaaaaaaaaaaaaaaaazzzzzzzzzzzzz\nxyz.cyz",
                    },
                    location: Some(
                        Location {
                            path: "/your mother.nix",
                            line: 1,
                            col: 4,
                        },
                    ),
                }
            "#]],
        );
    }
}

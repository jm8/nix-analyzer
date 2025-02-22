use std::path::PathBuf;

use anyhow::Result;

use crate::{
    evaluator::{proto::HoverRequest, Evaluator},
    file_types::FileInfo,
};

pub async fn hover(
    source: &str,
    file_info: &FileInfo,
    offset: u32,
    evaluator: &mut Evaluator,
) -> Result<HoverResult> {
    let yourmom = evaluator
        .hover(&HoverRequest {
            expression: "import".to_string(),
        })
        .await?;

    Ok(HoverResult {
        md: yourmom.value,
        position: None,
    })
}

pub struct Position {
    pub line: u32,
    pub col: u32,
    pub path: PathBuf,
}

pub struct HoverResult {
    pub md: String,
    pub position: Option<Position>,
}

#[cfg(test)]
mod test {

    use expect_test::{expect, Expect};
    use itertools::Itertools;

    use crate::{
        evaluator::Evaluator,
        file_types::{FileInfo, FileType},
    };

    use super::hover;

    async fn check_hover_with_filetype(source: &str, expected: Expect, file_type: &FileType) {
        let (left, right) = source.split("$0").collect_tuple().unwrap();
        let offset = left.len() as u32;

        let mut evaluator = Evaluator::new().await;

        let source = format!("{}{}", left, right);
        let actual = hover(
            &source,
            &FileInfo {
                file_type: file_type.clone(),
                path: "/test/test.nix".into(),
            },
            offset,
            &mut evaluator,
        )
        .await
        .unwrap();

        let actual = match actual.position {
            Some(position) => format!(
                "{}:{}:{}\n\n{}",
                position.path.display(),
                position.line,
                position.col,
                actual.md
            ),
            None => format!("no position\n\n{}", actual.md),
        };

        expected.assert_eq(&actual);
    }

    async fn check_hover(source: &str, expected: Expect) {
        check_hover_with_filetype(
            source,
            expected,
            &FileType::Custom {
                lambda_arg: "{}".to_string(),
                schema: "{}".to_string(),
            },
        )
        .await;
    }

    #[test_log::test(tokio::test)]
    async fn test_hover_builtin() {
        check_hover("imp$0ort", expect![[r#"
            no position

            ### built-in function `import` *`path `*


            Load, parse, and return the Nix expression in the file *path*.

            > **Note**
            >
            > Unlike some languages, `import` is a regular function in Nix.

            The *path* argument must meet the same criteria as an [interpolated expression](@docroot@/language/string-interpolation.md#interpolated-expression).

            If *path* is a directory, the file `default.nix` in that directory is used if it exists.

            > **Example**
            >
            > ```console
            > $ echo 123 > default.nix
            > ```
            >
            > Import `default.nix` from the current directory.
            >
            > ```nix
            > import ./.
            > ```
            >
            >     123

            Evaluation aborts if the file doesn’t exist or contains an invalid Nix expression.

            A Nix expression loaded by `import` must not contain any *free variables*, that is, identifiers that are not defined in the Nix expression itself and are not built-in.
            Therefore, it cannot refer to variables that are in scope at the call site.

            > **Example**
            >
            > If you have a calling expression
            >
            > ```nix
            > rec {
            >   x = 123;
            >   y = import ./foo.nix;
            > }
            > ```
            >
            >  then the following `foo.nix` will give an error:
            >
            >  ```nix
            >  # foo.nix
            >  x + 456
            >  ```
            >
            >  since `x` is not in scope in `foo.nix`.
            > If you want `x` to be available in `foo.nix`, pass it as a function argument:
            >
            >  ```nix
            >  rec {
            >    x = 123;
            >    y = import ./foo.nix x;
            >  }
            >  ```
            >
            >  and
            >
            >  ```nix
            >  # foo.nix
            >  x: x + 456
            >  ```
            >
            >  The function argument doesn’t have to be called `x` in `foo.nix`; any name would work.
        "#]]).await;
    }
}

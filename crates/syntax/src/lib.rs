use rowan::{GreenNodeBuilder, NodeOrToken};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[allow(non_camel_case_types)]
#[repr(u16)]

enum SyntaxKind {
    // TOKENS
    WHITESPACE = 0,
    ERROR,

    TOKEN_ID = 258,
    TOKEN_ATTRPATH = 259,
    TOKEN_STR = 260,
    TOKEN_IND_STR = 261,
    TOKEN_INT = 262,
    TOKEN_FLOAT = 263,
    TOKEN_PATH = 264,
    TOKEN_HPATH = 265,
    TOKEN_SPATH = 266,
    TOKEN_PATH_END = 267,
    TOKEN_URI = 268,
    TOKEN_IF = 269,
    TOKEN_THEN = 270,
    TOKEN_ELSE = 271,
    TOKEN_ASSERT = 272,
    TOKEN_WITH = 273,
    TOKEN_LET = 274,
    TOKEN_IN = 275,
    TOKEN_REC = 276,
    TOKEN_INHERIT = 277,
    TOKEN_EQ = 278,
    TOKEN_NEQ = 279,
    TOKEN_AND = 280,
    TOKEN_OR = 281,
    TOKEN_IMPL = 282,
    TOKEN_OR_KW = 283,
    TOKEN_DOLLAR_CURLY = 284,
    TOKEN_IND_STRING_OPEN = 285,
    TOKEN_IND_STRING_CLOSE = 286,
    TOKEN_ELLIPSIS = 287,
    TOKEN_LEQ = 288,
    TOKEN_GEQ = 289,
    TOKEN_UPDATE = 290,
    TOKEN_NOT = 291,
    TOKEN_CONCAT = 292,
    TOKEN_NEGATE = 293,

    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STRING,
    EXPR_PATH,
    EXPR_VAR,
    EXPR_SELECT,
    EXPR_OP_HAS_ATTR,
    EXPR_ATTRS,
    EXPR_LIST,
    EXPR_LAMBDA,
    EXPR_CALL,
    EXPR_LET,
    EXPR_WITH,
    EXPR_IF,
    EXPR_ASSERT,
    EXPR_OP_NOT,
    EXPR_OP_EQ,
    EXPR_OP_NEQ,
    EXPR_OP_AND,
    EXPR_OP_OR,
    EXPR_OP_IMPL,
    EXPR_OP_UPDATE,
    EXPR_OP_CONCAT_LISTS,
    EXPR_CONCAT_STRINGS,
    EXPR_POS,
    EXPR_BLACKHOLE,

    ROOT,
}
use SyntaxKind::*;

impl From<SyntaxKind> for rowan::SyntaxKind {
    fn from(kind: SyntaxKind) -> Self {
        Self(kind as u16)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
enum Lang {}
impl rowan::Language for Lang {
    type Kind = SyntaxKind;
    fn kind_from_raw(raw: rowan::SyntaxKind) -> Self::Kind {
        assert!(raw.0 <= SyntaxKind::ROOT as u16);
        unsafe { std::mem::transmute::<u16, SyntaxKind>(raw.0) }
    }
    fn kind_to_raw(kind: Self::Kind) -> rowan::SyntaxKind {
        kind.into()
    }
}

type SyntaxNode = rowan::SyntaxNode<Lang>;
#[allow(unused)]
type SyntaxToken = rowan::SyntaxToken<Lang>;
#[allow(unused)]
type SyntaxElement = rowan::NodeOrToken<SyntaxNode, SyntaxToken>;

fn print(indent: usize, element: SyntaxElement) {
    let kind: SyntaxKind = element.kind().into();
    print!("{:indent$}", "", indent = indent);
    match element {
        NodeOrToken::Node(node) => {
            println!("- {:?}", kind);
            for child in node.children_with_tokens() {
                print(indent + 2, child);
            }
        }

        NodeOrToken::Token(token) => println!("- {:?} {:?}", token.text(), kind),
    }
}

macro_rules! ast_node {
    ($ast:ident, $kind:ident) => {
        #[derive(PartialEq, Eq, Hash)]
        #[repr(transparent)]
        struct $ast(SyntaxNode);
        impl $ast {
            #[allow(unused)]
            fn cast(node: SyntaxNode) -> Option<Self> {
                if node.kind() == $kind {
                    Some(Self(node))
                } else {
                    None
                }
            }
        }
    };
}

ast_node!(Root, ROOT);
ast_node!(ExprInt, EXPR_INT);
ast_node!(ExprFloat, EXPR_FLOAT);
ast_node!(ExprString, EXPR_STRING);
ast_node!(ExprPath, EXPR_PATH);
ast_node!(ExprVar, EXPR_VAR);
ast_node!(ExprSelect, EXPR_SELECT);
ast_node!(ExprOpHasAttr, EXPR_OP_HAS_ATTR);
ast_node!(ExprAttrs, EXPR_ATTRS);
ast_node!(ExprList, EXPR_LIST);
ast_node!(ExprLambda, EXPR_LAMBDA);
ast_node!(ExprCall, EXPR_CALL);
ast_node!(ExprLet, EXPR_LET);
ast_node!(ExprWith, EXPR_WITH);
ast_node!(ExprIf, EXPR_IF);
ast_node!(ExprAssert, EXPR_ASSERT);
ast_node!(ExprOpNot, EXPR_OP_NOT);
ast_node!(ExprOpEq, EXPR_OP_EQ);
ast_node!(ExprOpNeq, EXPR_OP_NEQ);
ast_node!(ExprOpAnd, EXPR_OP_AND);
ast_node!(ExprOpOr, EXPR_OP_OR);
ast_node!(ExprOpImpl, EXPR_OP_IMPL);
ast_node!(ExprOpUpdate, EXPR_OP_UPDATE);
ast_node!(ExprOpConcatLists, EXPR_OP_CONCAT_LISTS);
ast_node!(ExprConcatStrings, EXPR_CONCAT_STRINGS);
ast_node!(ExprPos, EXPR_POS);
ast_node!(ExprBlackhole, EXPR_BLACKHOLE);

#[test]
fn test_syntax() {
    let syntax1 = {
        let mut builder = GreenNodeBuilder::new();

        builder.start_node(EXPR_LIST.into());

        builder.start_node(EXPR_INT.into());
        builder.token(TOKEN_INT.into(), "10");
        builder.finish_node();

        builder.start_node(EXPR_INT.into());
        builder.token(TOKEN_INT.into(), "20");
        builder.finish_node();

        builder.finish_node();

        let green_node = builder.finish();
        SyntaxNode::new_root(green_node)
    };

    let syntax2 = {
        let mut builder = GreenNodeBuilder::new();

        builder.start_node(EXPR_LIST.into());

        builder.start_node(EXPR_INT.into());
        builder.token(TOKEN_INT.into(), "5");
        builder.finish_node();

        builder.start_node(EXPR_INT.into());
        builder.token(TOKEN_INT.into(), "20");
        builder.finish_node();

        builder.finish_node();

        let green_node = builder.finish();
        SyntaxNode::new_root(syntax1.replace_with(green_node))
    };

    let a1 = syntax1.children().nth(0).unwrap();
    let b1 = syntax1.children().nth(1).unwrap();
    let a2 = syntax2.children().nth(0).unwrap();
    let b2 = syntax2.children().nth(1).unwrap();

    assert_ne!(a1.green(), a2.green());
    assert_eq!(b1.green(), b2.green());
}

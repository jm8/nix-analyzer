use std::path::PathBuf;

fn main() {
    let path = PathBuf::from(env!("NIX_EVAL_SERVER")).join("share/nix-eval-server.capnp");
    capnpc::CompilerCommand::new()
        .src_prefix(path.parent().unwrap())
        .file(path)
        .run()
        .unwrap();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    tonic_build::compile_protos(format!(
        "{}/share/nix-eval-server.proto",
        env!("NIX_EVAL_SERVER")
    ))?;
    Ok(())
}

{
  description = "nix-analyzer";

  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.rust-overlay.url = "github:oxalica/rust-overlay";
  inputs.nix-eval-server.url = "path:/var/home/josh/src/nix-analyzer-new/nix-eval-server";

  outputs = { self, flake-utils, rust-overlay, nix-eval-server, nixpkgs }: 
    flake-utils.lib.eachDefaultSystem (system: 
    let 
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import rust-overlay)];
      };
    in {
      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          (rust-bin.nightly.latest.default.override {
            extensions = ["rust-src"];
          })
        ];
        NIX_EVAL_SERVER = "${nix-eval-server.packages.${system}.nix-eval-server}/bin/nix-eval-server";
      };
    });
}


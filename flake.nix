{
  description = "nix-analyzer";

  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.rust-overlay.url = "github:oxalica/rust-overlay";
  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.home-manager.url = "github:nix-community/home-manager";
  inputs.nix-eval-server.url = "github:jm8/nix-eval-server";
  inputs.crane.url = "github:ipetkov/crane";

  outputs = {
    self,
    flake-utils,
    rust-overlay,
    nix-eval-server,
    nixpkgs,
    home-manager,
    crane,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {
        inherit system;
        overlays = [(import rust-overlay)];
      };
      craneLib = (crane.mkLib pkgs).overrideToolchain (p: p.rust-bin.nightly.latest.default.override {extensions = ["rust-src"];});

      commonArgs = {
        src = pkgs.lib.cleanSource ./.;
        strictDeps = true;

        nativeBuildInputs = with pkgs; [
          protobuf
        ];

        NIX_EVAL_SERVER = "${nix-eval-server.packages.${system}.nix-eval-server}";
        ALEJANDRA = "${pkgs.alejandra}/bin/alejandra";
        NIX_PATH = "nixpkgs=${nixpkgs}";
        NIXPKGS = nixpkgs;
        HOME_MANAGER = home-manager;
      };

      crate = craneLib.buildPackage (commonArgs
        // {
          cargoArtifacts = craneLib.buildDepsOnly commonArgs;
          doCheck = false;
        });
    in {
      packages.default = crate;

      devShells.default = craneLib.devShell ((builtins.removeAttrs commonArgs ["src" "strictDeps" "nativeBuildInputs"])
        // {
          buildInputs = commonArgs.nativeBuildInputs;
        });
    });
}

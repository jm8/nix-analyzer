{
  description = "nix-analyzer";

  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.rust-overlay.url = "github:oxalica/rust-overlay";
  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.home-manager.url = "github:nix-community/home-manager";
  inputs.nix-eval-server.url = "path:/var/home/josh/src/nix-analyzer-new/nix-eval-server";

  outputs = {
    self,
    flake-utils,
    rust-overlay,
    nix-eval-server,
    nixpkgs,
    home-manager,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
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
          protobuf
        ];
        NIX_EVAL_SERVER = "${nix-eval-server.packages.${system}.nix-eval-server}";
        ALEJANDRA = "${pkgs.alejandra}/bin/alejandra";
        NIX_PATH = "nixpkgs=${nixpkgs}";
        NIXPKGS = nixpkgs;
        HOME_MANAGER = home-manager;
      };
    });
}

{
  description = "Flake utils demo";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          nix-analyzer = pkgs.stdenv.mkDerivation {
            name = "nix-repl";
            src = ./.;
            boostInclude = "${pkgs.boost.dev}/include";
            buildInputs = with pkgs; [
              pkgconfig
              nixUnstable
              boehmgc
              boost
              nlohmann_json
              flex
              bison
            ];
            NIX_VERSION = (builtins.parseDrvName pkgs.nixUnstable.name).version;
            installPhase = ''
              mkdir -p $out/bin
              cp nix-analyzer $out/bin
            '';
          };
          default = nix-analyzer;
        };
      }
    );
}

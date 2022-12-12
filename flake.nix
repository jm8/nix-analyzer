{
  description = "A helpful nix language server";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nix.url = "path:./nix";

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    nix,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          nix-analyzer = pkgs.stdenv.mkDerivation {
            name = "nix-analyzer";
            src = ./.;
            boostInclude = "${pkgs.boost.dev}/include";
            nativeBuildInputs = with pkgs; [
              autoPatchelfHook
              pkgconfig
            ];
            buildInputs = with pkgs;
              [
                boehmgc
                boost
                nlohmann_json
                flex
                bison
              ]
              ++ nix.devShells.${system}.default.buildInputs
              ++ nix.devShells.${system}.default.nativeBuildInputs;
            installPhase = ''
              mkdir -p $out/bin
              cp nix-analyzer $out/bin
            '';
          };
          default = nix-analyzer;
        };
        devShells = rec {
          nix-analyzer = pkgs.mkShell {
            inputsFrom = [self.packages.${system}.default];
            boostInclude = "${pkgs.boost.dev}/include";
            shellHook = ''
              export LD_LIBRARY_PATH=$PWD/nix/build/lib
            '';
          };
          default = nix-analyzer;
        };
      }
    );
}

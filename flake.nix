{
  description = "A helpful nix language server";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixfork.url = "github:jm8/nix";

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    nixfork,
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
              ++ [
                nixfork.packages.${system}.default
              ];
            enableParalellBuilding = true;
            buildPhase = ''
              make
            '';
            installPhase = ''
              mkdir -p $out/{bin,lib}
              cp nix-analyzer-test $out/bin
            '';
            doCheck = true;
            checkPhase = ''
              ./nix-analyzer-test
            '';
          };
          default = nix-analyzer;
        };
      }
    );
}

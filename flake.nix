{
  description = "A helpful nix language server";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.nixfork.url = "github:jm8/nix";
  inputs.lspcppsrc = {
    url = "github:kuafuwang/LspCpp";
    flake = false;
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    nixfork,
    lspcppsrc,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          lspcpp = pkgs.stdenv.mkDerivation {
            name = "LspCpp";
            src = lspcppsrc;
            enableParallelBuilding = true;
            nativeBuildInputs = with pkgs; [
              cmake
            ];
            buildInputs = with pkgs; [
              boost
            ];
            installPhase = ''
              mkdir -p $out/{lib,include}
              cp liblspcpp.a $out/lib
              cp -r $src/include/LibLsp $out/include/LibLsp
              cp -r $src/third_party/uri/include/network $out/include/network
              cp third_party/uri/src/libnetwork-uri.a $out/lib
            '';
          };
          nix-analyzer = pkgs.stdenv.mkDerivation {
            name = "nix-analyzer";
            src = ./.;
            # not sure why boost isn't showing up in pkg-config
            boostInclude = "${pkgs.boost.dev}/include";
            boostLib = "${pkgs.boost}/lib";
            inherit lspcpp nixpkgs;
            nativeBuildInputs = with pkgs; [
              autoPatchelfHook
              pkgconfig
              python3
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
              cp nix-analyzer $out/bin
            '';
            doCheck = true;
            checkPhase = ''
              ./nix-analyzer-test ${nixpkgs}
            '';
          };
          default = nix-analyzer;
        };
      }
    );
}

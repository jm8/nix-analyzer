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
        system = "x86_64-linux";
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
            CFLAGS = [
              "--std=c++20"
              "-isystem${nixfork.packages.${system}.default.dev}/include"
              "-L${nixfork.packages.${system}.default}/lib"
              "-lnixmain -lnixexpr -lnixfetchers -lnixmain -lnixstore -lnixutil"
              "-isystem${pkgs.boost.dev}/include"
              "-L${pkgs.boost}/lib"
              "-isystem${pkgs.boehmgc.dev}/include"
              "-isystem${pkgs.nlohmann_json}/include"
            ];
            nixdebug = nixfork.packages.${system}.default.debug;
            nativeBuildInputs = with pkgs; [
              autoPatchelfHook
            ];
            enableParalellBuilding = true;
            buildPhase = ''
              make
            '';
            installPhase = ''
              mkdir -p $out/{bin,lib}
              cp nix-analyzer $out/bin
            '';
          };
          default = nix-analyzer;
        };
      }
    );
}

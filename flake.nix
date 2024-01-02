{
  description = "A helpful nix language server";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.flake-utils.url = "github:numtide/flake-utils";
  inputs.cpp-channel = {
    url = "github:andreiavrammsd/cpp-channel";
    flake = false;
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    cpp-channel,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        system = "x86_64-linux";
      in rec {
        packages = rec {
          nix-analyzer = pkgs.stdenv.mkDerivation {
            name = "nix-analyzer";
            src = ./.;
            CFLAGS = [
              "--std=c++20"
              "-isystem${pkgs.nix.dev}/include"
              "-I${pkgs.nix.dev}/include/nix" # needed for flake.hh
              "-L${pkgs.nix}/lib"
              "-lnixmain -lnixexpr -lnixfetchers -lnixstore -lnixutil"
              "-isystem${pkgs.boost.dev}/include"
              "-L${pkgs.boost}/lib"
              "-lboost_filesystem"
              "-isystem${pkgs.nlohmann_json}/include"
              "-isystem${pkgs.boehmgc.dev}/include"
              "-L${pkgs.boehmgc}/lib"
              "-lgc"
              "-isystem${pkgs.gtest.dev}/include"
              "-L${pkgs.gtest}/lib"
              "-isystem${pkgs.nlohmann_json}/include"
              "-isystem${cpp-channel}/include"
              ''-DHOMEMANAGERPATH="\"${pkgs.home-manager.src}\""''
            ];
            buildInputs = with pkgs; [
              boost
              nixfork.packages.x86_64-linux.default
              boehmgc
              gtest
              home-manager
            ];
            nativeBuildInputs = with pkgs; [
              autoPatchelfHook
            ];
            enableParalellBuilding = true;
            buildPhase = ''
              make
            '';
          };
          default = nix-analyzer;
        };
        devShells = {
          default = pkgs.mkShell {
            CFLAGS =
              packages.default.CFLAGS
              ++ [
                "-O0"
                "-g"
              ];
            nativeBuildInputs = with pkgs; [
              gdb
              valgrind
              bear
            ];
            inherit nixpkgs;
            NIX_PATH = "nixpkgs=${nixpkgs}";
          };
        };
      }
    );
}

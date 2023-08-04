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
      in rec {
        packages = rec {
          nix-analyzer = pkgs.stdenv.mkDerivation {
            name = "nix-analyzer";
            src = ./.;
            CFLAGS = [
              "--std=c++20"
              "-isystem${nixfork.packages.${system}.default.dev}/include"
              "-I${nixfork.packages.${system}.default.dev}/include/nix" # needed for flake.hh
              "-L${nixfork.packages.${system}.default}/lib"
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
            ];
            buildInputs = with pkgs; [
              boost
              nixfork.packages.x86_64-linux.default
              boehmgc
              gtest
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
            shellHook = ''
              echo directory ${nixfork} > ./.gdbinit
              echo set debug-file-directory ${nixfork.packages.${system}.default.debug}/lib/debug >> ./.gdbinit
            '';
          };
        };
      }
    );
}

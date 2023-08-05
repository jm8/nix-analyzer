{
  path,
  pkgs,
}: let
  schema =
    if pkgs.lib.strings.hasSuffix "/flake.nix" path
    then pkgs.lib.evalModules {modules = [./flakeSchema.nix];}
    else (import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];});
in
  schema // {_nixAnalyzerLoc = [];}

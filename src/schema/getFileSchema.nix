{
  path,
  pkgs,
}: let
  schema =
    if pkgs.lib.strings.hasSuffix "/flake.nix" path
    then import ./flakeSchema.nix {inherit pkgs;}
    else (import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];});
in
  schema // {_nixAnalyzerLoc = [];}

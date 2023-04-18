{
  path,
  pkgs,
}:
(import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];}) // {_nixAnalyzerLoc = [];}

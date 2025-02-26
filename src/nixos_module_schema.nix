let
  pkgs = import <nixpkgs> {};
in
  import ./module_to_schema.nix (import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];})

let
  pkgs = import <nixpkgs> {};
in
  import ./moduleToSchema.nix (import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];})

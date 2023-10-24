{
  path,
  pkgs,
  home_manager,
}: let
  schema =
    if pkgs.lib.strings.hasSuffix "/flake.nix" path
    then pkgs.lib.evalModules {modules = [./flakeSchema.nix];}
    else if builtins.getEnv "NIX_ANALYZER_HOME_MANAGER" != ""
    then
      pkgs.lib.evalModules {
        modules = import "${home_manager}/modules/modules.nix" {
          inherit pkgs;
          inherit (pkgs) lib;
        };
      }
    else (import (pkgs.path + "/nixos/lib/eval-config.nix") {modules = [];});
in
  schema // {_nixAnalyzerLoc = [];}

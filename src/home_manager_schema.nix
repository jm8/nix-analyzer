let
  pkgs = import <nixpkgs> {};
  hm_lib = import "${builtins.getEnv "HOME_MANAGER"}/modules/lib/stdlib-extended.nix" pkgs.lib;
  modules = import "${builtins.getEnv "HOME_MANAGER"}/modules/modules.nix" {
    inherit pkgs;
    lib = hm_lib;
    check = false;
    useNixpkgsModule = false;
  };
in
  import ./module_to_schema.nix (hm_lib.evalModules {inherit modules;})

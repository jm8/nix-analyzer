(
  let
    pkgs = import <nixpkgs> {};
  in
    pkgs.lib.evalModules {
      modules = [
        (
          {
            config,
            lib,
            pkgs,
            ...
          }:
            with lib; {
              options = {
                inputs = mkOption {
                  type = types.attrsOf (types.submodule ({
                    name,
                    config,
                    options,
                    ...
                  }: {
                    options = {
                      flake = mkOption {
                        type = types.bool;
                      };
                      url = mkOption {
                        type = types.string;
                      };
                    };
                  }));
                };
                outputs = mkOption {
                  type = types.anything;
                };
                description = mkOption {
                  type = types.str;
                };
              };
            }
        )
      ];
    }
)
.options

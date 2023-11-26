{
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  systemTo = elemType:
    types.submodule {
      options = listToAttrs (
        forEach systems.flakeExposed
        (system: {
          name = system;
          value = mkOption {
            type = elemType;
          };
        })
      );
    };
  attrsOfWithDefault = elemType:
    (types.attrsOf elemType) // {_nixAnalyzerIncludeDefault = true;};
in {
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
          type = mkOption {
            type = types.enum [
              "path"
              "git"
              "mercurial"
              "tarball"
              "file"
              "github"
              "gitlab"
              "sourcehut"
              "indirect"
            ];
          };
          url = mkOption {
            type = types.str;
            description = mdDoc "An attrset specifying the dependencies of the flake.";
          };
          dir = mkOption {
            type = types.str;
            description = mdDoc ''
              The subdirectory of the flake in which `flake.nix` is
              located. This parameter enables having multiple flakes in a
              repository or tarball. The default is the root directory of the
              flake.
            '';
          };
          narHash = mkOption {
            type = types.str;
            description = mdDoc ''
              The hash of the NAR serialisation (in SRI format) of the
              contents of the flake. This is useful for flake types such as
              tarballs that lack a unique content identifier such as a Git commit
              hash.
            '';
          };
          rev = mkOption {
            type = types.str;
            description = mdDoc ''
              A Git or Mercurial commit hash.
            '';
          };
          ref = mkOption {
            type = types.str;
            description = mdDoc ''
              A Git or Mercurial branch or tag name.
            '';
          };
          owner = mkOption {
            type = types.str;
            description = mdDoc ''
              The owner of the repository.
            '';
          };
          repo = mkOption {
            type = types.str;
            description = mdDoc ''
              The name of the repository.
            '';
          };
        };
      }));
      description = mdDoc ''
        Specifies the dependencies of a flake, as an
        attrset mapping input names to flake references.
      '';
    };
    outputs = mkOption {
      type = types.functionTo (types.submodule {
        options = {
          checks = mkOption {
            type = systemTo (attrsOfWithDefault types.package);
            description = mdDoc "Executed by `nix flake check`";
          };
          packages = mkOption {
            type = systemTo (attrsOfWithDefault types.package);
            description = mdDoc "Executed by `nix build .#<name>`";
          };
          apps = mkOption {
            type = systemTo (attrsOfWithDefault (types.submodule {
              options = {
                type = mkOption {
                  type = types.enum ["app"];
                };
                program = mkOption {
                  type = types.path;
                };
              };
            }));
            description = mdDoc "Executed by `nix run .#<name>``";
          };
          formatter = mkOption {
            type = systemTo types.package;
            description = mdDoc "Formatter (alejandra, nixfmt or nixpkgs-fmt)`";
          };
          legacyPackages = mkOption {
            type = systemTo (types.attrsOf types.package);
            description = mdDoc "Used for nixpkgs packages, also accessible via `nix build .#<name>`";
          };
          overlays = mkOption {
            type = attrsOfWithDefault types.raw;
            description = mdDoc "Overlays, consumed by other flakes";
          };
          nixosModules = mkOption {
            type = attrsOfWithDefault types.raw;
            description = mdDoc "Nixos modules, consumed by other flakes";
          };
          nixosConfigurations = mkOption {
            type = types.attrsOf types.raw;
            description = mdDoc "Used with `nixos-rebuild --flake .#<hostname>`";
          };
          devShells = mkOption {
            type = systemTo (attrsOfWithDefault types.package);
            description = mdDoc "Used by `nix develop .#<name>`";
          };
          hydraJobs = mkOption {
            type = types.attrsOf (systemTo types.package);
            description = mdDoc "Hydra build jobs";
          };
          templates = mkOption {
            type = attrsOfWithDefault (types.submodule {
              options = {
                path = mkOption {
                  type = types.path;
                };
                description = mkOption {
                  type = types.str;
                };
              };
            });
            description = "Used by `nix flake init -t <flake>#<name>`";
          };
        };
      });
    };
  };
}

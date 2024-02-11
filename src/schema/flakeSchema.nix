{
  type = "object";
  properties = {
    inputs = {
      additionalProperties = {
        type = "object";
        properties = {
          flake = {type = "boolean";};
          type = {
            enum = [
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
          url = {
            type = "string";
            description = "An attrset specifying the dependencies of the flake.";
          };
          dir = {
            type = "string";
            description = ''
              The subdirectory of the flake in which `flake.nix` is
              located. This parameter enables having multiple flakes in a
              repository or tarball. The default is the root directory of the
              flake.
            '';
          };
          narHash = {
            type = "string";
            description = ''
              The hash of the NAR serialisation (in SRI format) of the
              contents of the flake. This is useful for flake types such as
              tarballs that lack a unique content identifier such as a Git commit
              hash.
            '';
          };
          rev = {
            type = "string";
            description = ''
              A Git or Mercurial commit hash.
            '';
          };
          ref = {
            type = "string";
            description = ''
              A Git or Mercurial branch or tag name.
            '';
          };
          owner = {
            type = "string";
            description = ''
              The owner of the repository.
            '';
          };
          repo = {
            type = "string";
            description = ''
              The name of the repository.
            '';
          };
        };
      };
    };
    outputs = {};
  };
}
# inputs = mkOption {
#       type = types.attrsOf (types.submodule ({
#         name,
#         config,
#         options,
#         ...
#       }: {
#         options = {
#           flake = mkOption {
#             type = types.bool;
#           };
#           type = mkOption {
#             type = types.enum [
#               "path"
#               "git"
#               "mercurial"
#               "tarball"
#               "file"
#               "github"
#               "gitlab"
#               "sourcehut"
#               "indirect"
#             ];
#           };
#           url = mkOption {
#             type = types.str;
#             description = "An attrset specifying the dependencies of the flake.";
#           };
#           dir = mkOption {
#             type = types.str;
#             description = ''
#               The subdirectory of the flake in which `flake.nix` is
#               located. This parameter enables having multiple flakes in a
#               repository or tarball. The default is the root directory of the
#               flake.
#             '';
#           };
#           narHash = mkOption {
#             type = types.str;
#             description = ''
#               The hash of the NAR serialisation (in SRI format) of the
#               contents of the flake. This is useful for flake types such as
#               tarballs that lack a unique content identifier such as a Git commit
#               hash.
#             '';
#           };
#           rev = mkOption {
#             type = types.str;
#             description = ''
#               A Git or Mercurial commit hash.
#             '';
#           };
#           ref = mkOption {
#             type = types.str;
#             description = ''
#               A Git or Mercurial branch or tag name.
#             '';
#           };
#           owner = mkOption {
#             type = types.str;
#             description = ''
#               The owner of the repository.
#             '';
#           };
#           repo = mkOption {
#             type = types.str;
#             description = ''
#               The name of the repository.
#             '';
#           };
#         };
#       }));
#       description = ''
#         Specifies the dependencies of a flake, as an
#         attrset mapping input names to flake references.
#       '';
#     };
#     outputs = mkOption {
#       type = types.functionTo (types.submodule {
#         options = {
#           checks = mkOption {
#             type = systemTo (attrsOfWithDefault types.package);
#             description = "Executed by `nix flake check`";
#           };
#           packages = mkOption {
#             type = systemTo (attrsOfWithDefault types.package);
#             description = "Executed by `nix build .#<name>`";
#           };
#           apps = mkOption {
#             type = systemTo (attrsOfWithDefault (types.submodule {
#               options = {
#                 type = mkOption {
#                   type = types.enum ["app"];
#                 };
#                 program = mkOption {
#                   type = types.path;
#                 };
#               };
#             }));
#             description = "Executed by `nix run .#<name>``";
#           };
#           formatter = mkOption {
#             type = systemTo types.package;
#             description = "Formatter (alejandra, nixfmt or nixpkgs-fmt)`";
#           };
#           legacyPackages = mkOption {
#             type = systemTo (types.attrsOf types.package);
#             description = "Used for nixpkgs packages, also accessible via `nix build .#<name>`";
#           };
#           overlays = mkOption {
#             type = attrsOfWithDefault types.raw;
#             description = "Overlays, consumed by other flakes";
#           };
#           nixosModules = mkOption {
#             type = attrsOfWithDefault types.raw;
#             description = "Nixos modules, consumed by other flakes";
#           };
#           nixosConfigurations = mkOption {
#             type = types.attrsOf types.raw;
#             description = "Used with `nixos-rebuild --flake .#<hostname>`";
#           };
#           devShells = mkOption {
#             type = systemTo (attrsOfWithDefault types.package);
#             description = "Used by `nix develop .#<name>`";
#           };
#           hydraJobs = mkOption {
#             type = types.attrsOf (systemTo types.package);
#             description = "Hydra build jobs";
#           };
#           templates = mkOption {
#             type = attrsOfWithDefault (types.submodule {
#               options = {
#                 path = mkOption {
#                   type = types.path;
#                 };
#                 description = mkOption {
#                   type = types.str;
#                 };
#               };
#             });
#             description = "Used by `nix flake init -t <flake>#<name>`";
#           };
#         };
#       });
#     };
#   };
# }


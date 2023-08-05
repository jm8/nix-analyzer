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
          a = mkOption {
            type = types.str;
          };
          b = mkOption {
            type = types.str;
          };
        };
      });
    };
  };
}

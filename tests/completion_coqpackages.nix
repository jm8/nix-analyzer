import ./lib/maketest.nix {
  type = "completion";
  source = ''
    (import <nixpkgs> {}).coqPackages.aaa^
  '';
  expected = import ./lib/coqpackages.nix;
}
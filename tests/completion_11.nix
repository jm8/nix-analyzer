import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    (import <nixpkgs> {}).coqPackages.aaa^
  '';
  expected = import ./lib/coqpackages.nix;
}
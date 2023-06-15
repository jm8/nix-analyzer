import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {coqPackages}: coqPackages.^
  '';
  expected = import ./lib/coqpackages.nix;
}
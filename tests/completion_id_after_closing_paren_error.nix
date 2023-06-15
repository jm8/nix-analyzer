import ./lib/maketest.nix {
  type = "completion";
  source = ''
    (2+)^
  '';
  expected = import ./lib/builtinids.nix;
}
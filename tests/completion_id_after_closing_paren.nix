import ./lib/maketest.nix {
  type = "completion";
  source = ''
    (abc)^
  '';
  expected = import ./lib/builtinids.nix;
}
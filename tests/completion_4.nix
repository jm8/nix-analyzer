import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    (abc)^
  '';
  expected = import ./lib/builtinids.nix;
}
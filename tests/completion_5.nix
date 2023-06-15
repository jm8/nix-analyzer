import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    (2+)^
  '';
  expected = import ./lib/builtinids.nix;
}
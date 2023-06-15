import ./lib/maketest.nix {
  type = "completion";
  # disabled = true;
  source = ''
    with null; x^
  '';
  expected = import ./lib/builtinids.nix;
}
import ./lib/maketest.nix {
  type = "completion";
  source = ''
    map^
  '';
  expected = import ./lib/builtinids.nix;
}
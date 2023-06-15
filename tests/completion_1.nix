import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    map^
  '';
  expected = import ./lib/builtinids.nix;
}
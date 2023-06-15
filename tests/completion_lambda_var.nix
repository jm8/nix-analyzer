import ./lib/maketest.nix {
  type = "completion";
  source = ''
    {a, b, a}: a^
  '';
  expected = [
    "b"
    "a"
  ] ++ import ./lib/builtinids.nix;
}
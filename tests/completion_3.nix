import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {a, b, a}: a^
  '';
  expected = [
    "b"
    "a"
  ] ++ import ./lib/builtinids.nix;
}
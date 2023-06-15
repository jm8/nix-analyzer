import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    with {a = 2; b = 3;}; ^
  '';
  expected = [
    "b"
    "a"
  ] ++ import ./lib/builtinids.nix;
}
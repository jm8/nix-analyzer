import ./lib/maketest.nix {
  type = "completion";
  source = ''
    rec { a = ^; b = 2; }
  '';
  expected = [
    "b"
    "a"
  ] ++ import ./lib/builtinids.nix;
}
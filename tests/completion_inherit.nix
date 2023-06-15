import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let aaa = 2; in { inherit a^; }
  '';
  expected = [
    "aaa"
  ] ++ import ./lib/builtinids.nix;
}
import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    let aaa = 2; in { inherit a^; }
  '';
  expected = [
    "aaa"
  ] ++ import ./lib/builtinids.nix;
}
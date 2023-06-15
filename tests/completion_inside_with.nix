import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = 1; in { A = with ^; B = 2 }
  '';
  expected = [
    "a"
  ] ++ import ./lib/builtinids.nix;
}
import ./lib/maketest.nix {
  type = "parse";
  source = ''
    hel^lo
  '';
  expected = ''
    hello
  '';
  expectedErrors = [];
}

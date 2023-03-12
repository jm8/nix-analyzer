import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x.a.b^ = y; y = z; }
  '';
  expected = ''
    { x = { a = { b = y; }; }; y = z; }
  '';
  expectedErrors = [];
}

import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x^ = y; y = z; }
  '';
  expected = ''
    { x = y; y = z; }
  '';
  expectedErrors = [];
  expectedExprPath = ["ExprAttrs"];
}

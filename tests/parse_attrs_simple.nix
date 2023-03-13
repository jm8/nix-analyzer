import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x^ = y; y = z; }
  '';
  expected = ''
    { x = y; y = z; }
  '';
  expectedAttrPath = "x";
  expectedAttrPathIndex = 0;
  expectedErrors = [];
  expectedExprPath = ["ExprAttrs"];
}

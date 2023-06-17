import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { inherit o^r; }
  '';
  expected = ''
    { inherit or ; }
  '';
  expectedExprPath = ["ExprAttrs"];
}

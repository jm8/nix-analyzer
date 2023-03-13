import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {^ }
  '';
  expected = ''
    { }
  '';
  expectedExprPath = ["ExprAttrs"];
}

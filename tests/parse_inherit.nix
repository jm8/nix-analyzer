import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { inherit a^; inherit (x) b; }
  '';
  expected = ''
    { inherit a ; b = (x).b; }
  '';
  expectedExprPath = ["ExprAttrs"];
}

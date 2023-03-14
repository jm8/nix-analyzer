import ./lib/maketest.nix {
  type = "parse";
  source = ''
    x: y: x^
  '';
  expected = ''
    (x: (y: x))
  '';
  expectedExprPath = ["ExprVar" "ExprLambda" "ExprLambda"];
}

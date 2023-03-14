import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {}: ^a
  '';
  expected = ''
    ({  }: a)
  '';
  expectedExprPath = ["ExprVar" "ExprLambda"];
}

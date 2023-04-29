import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {a}: a^
  '';
  expected = ''
    ({ a }: a)
  '';
  expectedExprPath = ["ExprVar" "ExprLambda"];
}

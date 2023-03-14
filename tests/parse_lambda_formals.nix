import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {a, ^b}: a
  '';
  expected = ''
    ({ a, b }: a)
  '';
  expectedFormal = "b";
  expectedExprPath = ["ExprLambda"];
}

import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {a, ^b, a}: a
  '';
  expected = ''
    ({ a, b }: a)
  '';
  expectedFormal = "b";
  expectedErrors = ["duplicate formal function argument 0:7-0:8"];
  expectedExprPath = ["ExprLambda"];
}

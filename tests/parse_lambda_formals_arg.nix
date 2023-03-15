import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {a, b, ...} @ ^g: a
  '';
  expected = ''
    ({ a, b, ... } @ g: a)
  '';
  expectedArg = true;
  expectedExprPath = ["ExprLambda"];
}

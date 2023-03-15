import ./lib/maketest.nix {
  type = "parse";
  source = ''
    g@{a, b^, ...}: a
  '';
  expected = ''
    ({ a, b, ... } @ g: a)
  '';
  expectedFormal = "b";
  expectedExprPath = ["ExprLambda"];
}

import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x = 123; y = 456; x = 789; }^
  '';
  expected = ''
    { x = 123; y = 456; }
  '';
  expectedErrors = ["duplicate attr 0:20-0:21"];
  expectedExprPath = ["ExprAttrs"];
}

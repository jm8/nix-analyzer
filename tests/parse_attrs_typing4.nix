import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x.y =^ }
  '';
  expected = ''
    { x = { y = null; }; }
  '';
  expectedErrors = ["expected expression 0:7-0:8"];
  expectedExprPath = ["ExprVar" "ExprAttrs"];
}

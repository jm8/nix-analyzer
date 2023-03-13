import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x^ }
  '';
  expected = ''
    { }
  '';
  expectedErrors = ["expected '=', got '}' 0:4-0:6"];
  expectedExprPath = ["ExprAttrs"];
}

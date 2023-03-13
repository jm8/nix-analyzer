import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x.y^ }
  '';
  expected = ''
    { }
  '';
  expectedAttrPath = ''x.y'';
  expectedAttrPathIndex = 1;
  expectedErrors = ["expected '=', got '}' 0:6-0:8"];
  expectedExprPath = ["ExprAttrs"];
}

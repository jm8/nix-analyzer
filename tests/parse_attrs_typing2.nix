import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x.^ }
  '';
  expected = ''
    { }
  '';
  expectedAttrPath = ''x.""'';
  expectedAttrPathIndex = 1;
  expectedErrors = ["expected ID 0:5-0:7" "expected '=', got '}' 0:5-0:7"];
  expectedExprPath = ["ExprAttrs"];
}

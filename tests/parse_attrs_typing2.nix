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
  expectedErrors = ["expected ID 0:5-0:6"];
  expectedExprPath = ["ExprAttrs"];
}

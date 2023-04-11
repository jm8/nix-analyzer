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
  expectedErrors = ["expected ID 0:3-0:5"];
  expectedExprPath = ["ExprAttrs"];
}

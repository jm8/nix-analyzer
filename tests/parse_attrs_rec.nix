import ./lib/maketest.nix {
  type = "parse";
  source = ''
    rec { ^ }
  '';
  expected = ''
    rec { }
  '';
  expectedErrors = [];
  expectedExprPath = ["ExprAttrs"];
}

import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { x.y = abc^ }
  '';
  expected = ''
    { x = { y = abc; }; }
  '';
  expectedErrors = ["expected ';', got '}' 0:12-0:14"];
  expectedExprPath = ["ExprVar" "ExprAttrs"];
}

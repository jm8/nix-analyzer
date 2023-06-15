import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {a = 1, b = 2^}
  '';
  expected = ''
    { a = 1; b = 2; }
  '';
  expectedExprPath = ["ExprInt" "ExprAttrs"];
  expectedErrors = ["expected ';', got ',' 0:6-0:7" "expected ';', got '}' 0:13-0:14"];
}
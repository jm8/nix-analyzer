import ./lib/maketest.nix {
  type = "parse";
  source = ''
    assert x; ^y
  '';
  expected = ''
    assert x; y
  '';
  expectedExprPath = ["ExprVar" "ExprAssert"];
}

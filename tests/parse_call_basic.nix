import ./lib/maketest.nix {
  type = "parse";
  source = ''
    fun^ction a b c d
  '';
  expected = ''
    (function (a (b (c d))))
  '';
  expectedExprPath = ["ExprVar" "ExprCall"];
}

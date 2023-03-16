import ./lib/maketest.nix {
  type = "parse";
  source = ''
    a + b * c^ + d
  '';
  expected = ''
    (a + ((__mul b c) + d))
  '';
  expectedExprPath = [ "ExprVar" "ExprCall" "ExprConcatStrings" "ExprConcatStrings" ];
}

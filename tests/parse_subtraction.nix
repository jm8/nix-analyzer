import ./lib/maketest.nix {
  type = "parse";
  source = ''
    - ^n - 1
  '';
  expected = ''
    (__sub (__sub 0 n) 1)
  '';
  expectedErrors = [];
  expectedExprPath = [ "ExprVar" "ExprCall" "ExprCall" ];
}

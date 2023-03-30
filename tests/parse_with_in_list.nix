import ./lib/maketest.nix {
  type = "parse";
  source = ''
    [ a b with c; ^d e]
  '';
  expected = ''
    [ (a) (b) ((with c; d)) (e) ]
  '';
  expectedErrors = [ "WITH not allowed here 0:6-0:10" ];
  expectedExprPath = [ "ExprVar" "ExprWith" "ExprList" ];
}

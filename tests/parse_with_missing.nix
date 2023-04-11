import ./lib/maketest.nix {
  type = "parse";
  source = ''
    with pkgs; ^
  '';
  expected = ''
    (with pkgs; null)
  '';
  expectedErrors = [ "expected expression 0:10-1:0" ];
  expectedExprPath = [ "ExprVar" "ExprWith" ];
}

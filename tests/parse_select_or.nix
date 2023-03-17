import ./lib/maketest.nix {
  type = "parse";
  source = ''
    or.foo or foo^
  '';
  expected = ''
    (or).foo or (foo)
  '';
  expectedExprPath = [ "ExprVar" "ExprSelect" ];
}

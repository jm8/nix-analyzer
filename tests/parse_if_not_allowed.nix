import ./lib/maketest.nix {
  type = "parse";
  source = ''
    ^a if b then c else d
  '';
  expected = ''
    (a (if b then c else d))
  '';
  expectedExprPath = [ "ExprVar" "ExprCall" ];
  expectedErrors = [ "IF not allowed here 0:2-0:4" ];
}

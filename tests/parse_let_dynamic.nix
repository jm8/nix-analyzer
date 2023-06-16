import ./lib/maketest.nix {
  type = "parse";
  source = ''
    let ''${a} = b; in c^
  '';
  expected = ''
    (let in c)
  '';
  expectedExprPath = [ "ExprVar" "ExprLet" ];
  expectedErrors = [ "dynamic attrs are not allowed in let 0:4-0:8" ];
}

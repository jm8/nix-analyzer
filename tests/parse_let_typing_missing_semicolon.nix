import ./lib/maketest.nix {
  type = "parse";
  source = ''
    let a = 4 in b^
  '';
  expected = ''
    (let a = 4; in b)
  '';
  expectedErrors = ["expected ';', got 'IN' 0:10-0:12"];
  expectedExprPath = ["ExprVar" "ExprLet"];
}

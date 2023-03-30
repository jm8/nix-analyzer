import ./lib/maketest.nix {
  type = "parse";
  source = ''
    let a = 4; b^
  '';
  expected = ''
    (let a = 4; in b)
  '';
  expectedErrors = ["expected 'IN', got 'ID' 0:11-0:12"];
  expectedExprPath = ["ExprVar" "ExprLet"];
}

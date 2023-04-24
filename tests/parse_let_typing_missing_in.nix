import ./lib/maketest.nix {
  type = "parse";
  source = ''
    let a = 4; b^
  '';
  expected = ''
    (let a = 4; in null)
  '';
  expectedAttrPath = "b";
  expectedAttrPathIndex = 0;
  expectedErrors = ["expected '=', got 'EOF' 1:0-1:0"];
  # probably shouldn't have exprvar
  expectedExprPath = ["ExprVar" "ExprLet"];
}

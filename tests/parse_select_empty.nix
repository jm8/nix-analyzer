import ./lib/maketest.nix {
  type = "parse";
  source = ''
    hello.whatever.^
  '';
  expected = ''
    (hello).whatever.""
  '';
  expectedAttrPath = ''whatever.""'';
  expectedAttrPathIndex = 1;
  expectedErrors = ["expected ID 0:14-1:0"];
  expectedExprPath = ["ExprSelect"];
}

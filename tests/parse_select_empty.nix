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
  expectedErrors = ["expected ID 0:16-0:16"];
  expectedExprPath = ["ExprSelect"];
}

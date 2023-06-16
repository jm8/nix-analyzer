import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {
      "^a" = b;
    }
  '';
  expected = ''
    { a = b; }
  '';
  expectedAttrPath = "a";
  expectedAttrPathIndex = 0;
  expectedExprPath = [ "ExprAttrs" ];
}

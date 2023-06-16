import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {
      ''${^a} = b;
    }
  '';
  expected = ''
    { "''${a}" = b; }
  '';
  expectedExprPath = [ "ExprVar" "ExprAttrs" ];
}

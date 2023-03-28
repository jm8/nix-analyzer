import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    ./parse_assert.nix^
  '';
  expected = ''
    /base-path/parse_assert.nix
  '';
  expectedExprPath = [ "ExprPath" ];
}


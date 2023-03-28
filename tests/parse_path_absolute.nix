import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    /etc/passwd^
  '';
  expected = ''
    /etc/passwd
  '';
  expectedExprPath = [ "ExprPath" ];
}


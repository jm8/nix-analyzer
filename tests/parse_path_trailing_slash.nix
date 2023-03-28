import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    /home/
  '';
  expected = ''
    /home/
  '';
  expectedExprPath = [ "ExprPath" ];
}


import ./lib/maketest.nix
{
  type = "parse";
  disabled = true;
  source = ''
    /home/
  '';
  expected = ''
    /home/
  '';
  expectedExprPath = ["ExprPath"];
}

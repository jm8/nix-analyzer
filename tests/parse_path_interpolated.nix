import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    /home/''${username^}/whatever
  '';
  expected = ''
    (/home/ + username + "/whatever")
  '';
  expectedExprPath = [ "ExprVar" "ExprConcatStrings" ];
}


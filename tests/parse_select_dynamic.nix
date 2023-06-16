import ./lib/maketest.nix {
  type = "parse";
  source = ''
    hello.''${1^2}
  '';
  expected = ''
    (hello)."''${12}"
  '';
  expectedExprPath = [ "ExprInt" "ExprSelect" ];
}

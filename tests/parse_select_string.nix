import ./lib/maketest.nix {
  type = "parse";
  source = ''
    hello."ab^c"."x''${yz}"
  '';
  expected = ''
    (hello).abc."''${("x" + yz)}"
  '';
  expectedAttrPath = ''abc."''${("x" + yz)}"'';
  expectedAttrPathIndex = 0;
  expectedExprPath = [ "ExprSelect" ];
}
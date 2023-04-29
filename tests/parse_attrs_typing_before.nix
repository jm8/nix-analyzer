import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {
      services.resolved.^
      networking.useDHCP = false;
    }
  '';
  expected = ''
    { services = { resolved = { networking = { useDHCP = false; }; }; }; }
  '';
  expectedAttrPathIndex = 2;
  expectedAttrPath = "services.resolved.networking.useDHCP";
  expectedExprPath = [ "ExprAttrs" ];
}

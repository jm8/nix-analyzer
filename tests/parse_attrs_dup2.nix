import ./lib/maketest.nix {
  type = "parse";
  source = ''
    { services.ssh.port = 22; services.ssh^.port = 23; }
  '';
  expected = ''
    { services = { ssh = { port = 22; }; }; }
  '';
  expectedAttrPath = "services.ssh.port";
  expectedAttrPathIndex = 1;
  expectedErrors = ["duplicate attr 0:26-0:43"];
  expectedExprPath = ["ExprAttrs"];
}

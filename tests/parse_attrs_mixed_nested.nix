import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {
      services.ssh.enable = true;
      services.ssh = { port = 123; };
      services = {
          httpd.enable = true;
      };
    }
  '';
  expected = ''
    { services = { httpd = { enable = true; }; ssh = { enable = true; port = 123; }; }; }
  '';
  expectedErrors = [];
  expectedExprPath = ["ExprAttrs"];
}

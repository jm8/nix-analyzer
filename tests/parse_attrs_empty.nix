import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {^ }
  '';
  expected = ''
    { }
  '';
  expectedErrors = [];
}

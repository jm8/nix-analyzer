import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {a = 2; a = 3;}^
  '';
  expected = [
  ];
}
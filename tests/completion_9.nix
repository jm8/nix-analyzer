import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {a = 1, b = 2}^
  '';
  expected = [
  ];
}
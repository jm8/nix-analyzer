import ./lib/maketest.nix {
  type = "completion";
  source = ''
    {a = 2; a = 3;}.
  '';
  expected = [
    "a"
  ];
}
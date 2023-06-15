import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    ({ colors.red = 0; colors.green = 100; somethingelse = -1; }.colors.^)
  '';
  expected = [
    "green"
    "red"
  ];
}
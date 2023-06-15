import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    { "" = { a = 1; }; }..^
  '';
  expected = [
    "a"
  ];
}
import ./lib/maketest.nix {
  type = "completion";
  source = ''
    {apple = 4; banana = 7; }.a^
  '';
  expected = [
    "banana"
    "apple"
  ];
}
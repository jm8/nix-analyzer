import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {apple = 4; banana = 7; }.a^
  '';
  expected = [
    "banana"
    "apple"
  ];
}
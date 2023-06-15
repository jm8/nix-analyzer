import ./lib/maketest.nix {
  type = "completion";
  source = ''
    ({a, b}: a) { ^}
  '';
  expected = [
    "b"
    "a"
  ];
}
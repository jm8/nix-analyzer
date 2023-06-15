import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    ({a, b}: a) { ^}
  '';
  expected = [
    "b"
    "a"
  ];
}
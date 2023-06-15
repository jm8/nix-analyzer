import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = { b = 3; }; in { inherit (a) x^; }
  '';
  expected = [
    "b"
  ];
}
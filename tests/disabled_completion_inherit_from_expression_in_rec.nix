import ./lib/maketest.nix {
  type = "completion";
  # bug
  disabled = true;
  source = ''
    let a = { b = 3; }; in rec { inherit (a) x^; }
  '';
  expected = [
    "b"
  ];
}
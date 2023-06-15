import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = {b = x: x; }; in a.^ null
  '';
  expected = [
    "b"
  ];
}
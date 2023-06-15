import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = {b = 2;}; c = a.^ in a
  '';
  expected = [
    "b"
  ];
}
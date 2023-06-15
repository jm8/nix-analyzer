import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    let a = {b = 2;}; c = a.^ in a
  '';
  expected = [
    "b"
  ];
}
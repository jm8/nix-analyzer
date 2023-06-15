import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    let a = {b = x: x; }; in a.^ null
  '';
  expected = [
    "b"
  ];
}
import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    { xdg.autostart.^ = {}; }
  '';
  expected = [
    "enable"
  ];
}
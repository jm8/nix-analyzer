import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { xdg.autostart = { ^ }; }
  '';
  expected = [
    "enable"
  ];
}
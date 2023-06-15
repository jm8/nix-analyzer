import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
     { xdg = { ^ }; }
  '';
  expected = [
    "menus"
    "icons"
    "mime"
    "portal"
    "sounds"
    "autostart"
  ];
}
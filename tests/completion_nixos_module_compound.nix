import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { xdg.^ }
  '';
  expected = [
    "menus"
    "icons"
    "mime"
    "portal"
    "sounds"
    "autostart"
  ];
  ftype = {
    schema = import ../src/schema/nixosModuleSchema.nix;
  };
}

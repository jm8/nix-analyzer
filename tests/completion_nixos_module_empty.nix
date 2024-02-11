import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { xdg.autostart.enable = {^}; }
  '';
  expected = [
  ];
  ftype = {
    schema = import ../src/schema/nixosModuleSchema.nix;
  };
}

import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { xdg.autostart = { ^ }; }
  '';
  expected = [
    "enable"
  ];
  ftype = {
    schema = import ../src/schema/nixosModuleSchema.nix;
  };
}

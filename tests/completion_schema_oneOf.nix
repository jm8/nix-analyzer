import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { ^ }
  '';
  ftype = {
    schema = {
      properties = {"abc" = {};};
      oneOf = [
        {properties = {"def" = {};};}
        {properties = {"hij" = {};};}
      ];
    };
  };
  expected = [
    "abc"
    "def"
    "hij"
  ];
}

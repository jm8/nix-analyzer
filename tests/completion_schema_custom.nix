import ./lib/maketest.nix {
  type = "completion";
  source = ''
     { ^ }
  '';
  ftype = {
    schema = {
      properties = {
        "abc" = {};
      };
    };
  };
  expected = [
    "abc"
  ];
}

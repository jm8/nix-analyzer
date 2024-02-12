import ./lib/maketest.nix {
  type = "diagnostic";
  source = ''
    let
      a = 123;
      b = throw "canyouseeme";
    in
      a
  '';
  expected = [
    "canyouseeme 2:6-2:25"
  ];
}



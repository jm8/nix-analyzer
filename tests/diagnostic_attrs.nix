import ./lib/maketest.nix {
  type = "diagnostic";
  source = ''
    { a = 2; b = throw "yikes"; }
  '';
  expected = [
    "yikes 0:13-0:26"
  ];
}


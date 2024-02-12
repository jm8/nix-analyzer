import ./lib/maketest.nix {
  type = "diagnostic";
  source = ''
    2 + "hello"
  '';
  expected = [
    "cannot add a string to an integer 0:0-0:11"
  ];
}

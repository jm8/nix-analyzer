import ./lib/maketest.nix {
  type = "completion";
  source = ''
    undefinedvariable.^
  '';
  expected = [
  ];
}
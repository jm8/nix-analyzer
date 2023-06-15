import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    undefinedvariable.^
  '';
  expected = [
  ];
}
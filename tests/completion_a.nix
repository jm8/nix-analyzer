import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = {x = 1; y = 2;}.^
  '';
  expected = [
    "x"
    "y"
  ];
}
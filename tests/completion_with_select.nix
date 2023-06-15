import ./lib/maketest.nix {
  type = "completion";
  source = ''
    with { a = {b = 1; }; }; a.que^
  '';
  expected = [
    "b"
  ];
}
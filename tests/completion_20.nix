import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    with { a = {b = 1; }; }; a.que^
  '';
  expected = [
    "b"
  ];
}
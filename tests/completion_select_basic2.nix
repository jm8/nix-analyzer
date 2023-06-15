import ./lib/maketest.nix {
  type = "completion";
  source = ''
    {abc = 2; def = "green";}.^
  '';
  expected = [
    "def"
    "abc"
  ];
}
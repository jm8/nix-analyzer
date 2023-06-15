import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    {abc = 2; def = "green";}.^
  '';
  expected = [
    "def"
    "abc"
  ];
}
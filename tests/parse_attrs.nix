rec {
  type = "parse";
  source = ''
    { x = y; y = z; }
  '';
  expected = source;
  expectedErrors = [];
}

{
  type = "parse";
  source = ''
    { x = y; y = z; }
  '';
  expected = ''
    null
  '';
}

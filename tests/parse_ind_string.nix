import ./lib/maketest.nix {
  type = "parse";
  source = ''
    ${"''"}
      abc
      ${"$"}{hello world}
    ${"''"}
  '';
  expected = ''
    ("abc\n" + (hello world) + "\n")
  '';
  expectedExprPath = [ "ExprConcatStrings" ];
}

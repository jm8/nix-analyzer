import ./lib/maketest.nix {
  type = "parse";
  source = ''
    ${"''"}
      abc
      ${"$"}{hello}
    ${"''"}
  '';
  expected = ''
    ("abc\n" + hello + "\n")
  '';
  expectedExprPath = [ "ExprConcatStrings" ];
}

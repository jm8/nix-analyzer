import ./lib/maketest.nix {
  type = "parse";
  source = ''
    "hello ''${abc} ''${^}"
  '';
  expected = ''
    ("hello " + abc + " " + null)
  '';
  expectedErrors = [ "expected expression 0:16-0:16" ];
  expectedExprPath = [ "ExprVar" "ExprConcatStrings" ];
}

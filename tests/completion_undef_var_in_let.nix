import ./lib/maketest.nix {
  type = "completion";
  source = ''
     let a = undefinedvariable; in aaa^
  '';
  expected = [
    "a"
  ] ++ import ./lib/builtinids.nix;
}
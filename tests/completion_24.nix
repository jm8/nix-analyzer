import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
     let a = undefinedvariable; in aaa^
  '';
  expected = [
    "a"
  ] ++ import ./lib/builtinids.nix;
}
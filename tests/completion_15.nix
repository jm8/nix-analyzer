import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    rec { a = [ ^ ]; b = 2; }
  '';
  expected = [
    "b"
    "a"
  ] ++ import "./lib/builtinids";
}
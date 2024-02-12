import ./lib/maketest.nix {
  type = "diagnostic";
  source = ''
    builtins.removeAttrs ["not an attrset"] ["x" "y" "z"]
  '';
  expected = [
    "value is a list while a set was expected 0:0-0:53"
  ];
}


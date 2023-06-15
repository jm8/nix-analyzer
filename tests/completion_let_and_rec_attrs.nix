import ./lib/maketest.nix {
  type = "completion";
  source = ''
    let a = { b = 3; }; in rec { s = [ a.aaa^ ]; }
  '';
  expected = [
    "b"
  ];
}
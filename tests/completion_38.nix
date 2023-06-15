import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  source = ''
    let a = { b = 3; }; in { inherit (a) ^; }
  '';
  expected = [
    "b"
  ];
}
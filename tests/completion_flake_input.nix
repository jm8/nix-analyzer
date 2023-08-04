import ./lib/maketest.nix {
  type = "completion";
  path = "/flake.nix";
  source = ''
    {
      inputs = {
        whatever = {
          ^
        };
      };
    }
  '';
  expected = [
    "dir"
    "flake"
    "narHash"
    "owner"
    "ref"
    "repo"
    "rev"
    "type"
    "url"
  ];
}

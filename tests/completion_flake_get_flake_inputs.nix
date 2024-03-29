import ./lib/maketest.nix {
  type = "completion";
  path = "/flake.nix";
  source = ''
    {
      inputs.flake-utils.url = "github:numtide/flake-utils?rev=919d646de7be200f3bf08cb76ae1f09402b6f9b4";

      outputs = {flake-utils}: flake-utils.lib.^
    }
  '';
  expected = [
    "allSystems"
    "check-utils"
    "defaultSystems"
    "eachDefaultSystem"
    "eachDefaultSystemMap"
    "eachSystem"
    "eachSystemMap"
    "filterPackages"
    "flattenTree"
    "meld"
    "mkApp"
    "simpleFlake"
    "system"
  ];
}

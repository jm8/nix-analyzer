import ./lib/maketest.nix {
  type = "completion";
  source = ''
    { ^}
  '';
  path = "/etc/configuration.nix";
  expected = [
  "appstream"
  "assertions"
  "boot"
  "console"
  "containers"
  "docker-containers"
  "documentation"
  "dysnomia"
  "ec2"
  "environment"
  "fileSystems"
  "fonts"
  "gtk"
  "hardware"
  "i18n"
  "ids"
  "jobs"
  "krb5"
  "lib"
  "location"
  "meta"
  "nesting"
  "networking"
  "nix"
  "nixops"
  "nixpkgs"
  "oci"
  "openstack"
  "passthru"
  "power"
  "powerManagement"
  "programs"
  "qt"
  "qt5"
  "security"
  "services"
  "snapraid"
  "sound"
  "specialisation"
  "stubby"
  "swapDevices"
  "system"
  "systemd"
  "time"
  "users"
  "virtualisation"
  "warnings"
  "xdg"
  "zramSwap"
]
;
  ftype = {
    schema = import ../src/schema/nixosModuleSchema.nix;
  };
}

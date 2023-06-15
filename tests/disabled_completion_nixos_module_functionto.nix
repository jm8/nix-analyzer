import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
  # not implemented yet
  source = ''
    { programs.ssh.knownHosts.myhost.what^ = null; }
  '';
  expected = [
    "extraHostNames"
    "hostNames"
    "publicKeyFile"
    "publicKey"
    "certAuthority"
  ];
}
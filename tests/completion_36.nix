import ./lib/maketest.nix {
  type = "completion";
  disabled = true;
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
import ./lib/maketest.nix {
  type = "parse";
  source = ''
    (import /nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source).^
  '';
  expected = ''
    ((import /nix/store/xif4dbqvi7bmcwfxiqqhq0nr7ax07liw-source)).""
  '';
  expectedAttrPath = ''""'';
  expectedAttrPathIndex = 0;
  expectedExprPath = ["ExprSelect"];
  expectedErrors = ["expected ID 0:59-1:0"];
}

import ./lib/maketest.nix {
  type = "completion";
  # not implemented yet
  disabled = true;
  source = ''
    {graphviz}: graphviz.override { ^ }
  '';
  expected = [
    "xorg"
    "pkg-config"
    "bison"
    "ApplicationServices"
    "fetchFromGitLab"
    "libpng"
    "expat"
    "stdenv"
    "bash"
    "fltk"
    "flex"
    "pango"
    "libjpeg"
    "python3"
    "autoreconfHook"
    "fetchpatch"
    "gd"
    "lib"
    "gts"
    "withXorg"
    "libdevil"
    "cairo"
    "exiv2"
    "fontconfig"
    "libtool"
  ];
}
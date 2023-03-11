{
  function,
  name,
}: let
  pkgs = import <nixpkgs> {};
in
  if name == "mkDerivation"
  then {
    buildFlags = "whatever";
  }
  else if pkgs.lib.isFunction function
  then pkgs.lib.functionArgs function
  else {myNameIs = name;}
# attrs: attrs // {here = true;}


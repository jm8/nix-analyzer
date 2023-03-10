# {
#   function,
#   name,
# }: let
#   pkgs = import <nixpkgs> {};
# in
#   if name == "mkDerivation"
#   then {
#     buildFlags = "whatever";
#   }
#   else pkgs.lib.functionArgs function
attrs: attrs."name"

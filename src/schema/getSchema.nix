{
  # the path to the file
  path,
  # same length as exprpath
  # each element is `null` (if the expr is not a call),
  # or `{ function = <function object>; name = "<function name>"; }`
  functions,
}: let
  pkgs = import <nixpkgs> {};
in
  functions

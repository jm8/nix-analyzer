{
  pkgs,
  parent,
}:
pkgs.lib.mapAttrs (name: value:
    if parent ? _nixAnalyzerLoc
    then value // {_nixAnalyzerLoc = parent._nixAnalyzerLoc ++ [name];}
    else value) (
  if parent ? type
  then
    (
      if parent.type.name == "attrsOf"
      then parent.type.nestedTypes.elemType.getSubOptions []
      else if parent.type.name == "submodule"
      then parent.type.getSubOptions []
      else {}
    )
  else parent
)

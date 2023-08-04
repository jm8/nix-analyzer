{
  pkgs,
  parent,
}: let
  addLocToAttrs = {
    attrs,
    parentLoc,
  }:
    pkgs.lib.mapAttrs (
      name: value:
        value // {_nixAnalyzerLoc = parentLoc ++ [name];}
    )
    attrs;
in (
  if parent ? type.name
  then
    (
      if parent.type.name == "attrsOf"
      then let
        options = parent.type.nestedTypes.elemType.getSubOptions [];
        optionsWithLocs = addLocToAttrs {
          attrs = options;
          parentLoc = parent._nixAnalyzerLoc ++ ["<?>"];
        };
      in {
        _nixAnalyzerLoc = parent._nixAnalyzerLoc ++ ["<?>"];
        _nixAnalyzerAttrsOf = optionsWithLocs // {_nixAnalyzerLoc = parent._nixAnalyzerLoc ++ ["<?>"];};
      }
      else if parent.type.name == "submodule"
      then
        addLocToAttrs {
          parentLoc = parent._nixAnalyzerLoc;
          attrs = parent.type.getSubOptions [];
        }
      else {}
    )
  else
    addLocToAttrs {
      parentLoc = parent._nixAnalyzerLoc;
      attrs = parent;
    }
)

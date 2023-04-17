{
  pkgs,
  parent,
}:
  if parent ? type then
    (if parent.type.name == "attrsOf" then
      parent.type.nestedTypes.elemType.getSubOptions []
    else if parent.type.name == "submodule" then
      parent.type.getSubOptions []
    else {})
  else
    parent

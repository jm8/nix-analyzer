{
  pkgs,
  symbol,
  parent,
}:
  if parent ? type then
    if parent.type.name == "attrsOf" then
      parent.type.nestedTypes.elemType.getSubOptions []
    else
      {}
  else
    parent.${symbol}
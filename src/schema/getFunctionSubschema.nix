{
  pkgs,
  schema,
}:
if schema ? type && schema.type.name == "functionTo"
then {
  type = schema.type.nestedTypes.elemType;
  _nixAnalyzerLoc = schema._nixAnalyzerLoc;
}
else {}

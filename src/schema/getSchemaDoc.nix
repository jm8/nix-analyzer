{
  schema,
  pkgs
}:

let getDoc = schema: (if schema ? _type then
  ''
    option `${builtins.concatStringsSep "." schema.loc}`
    ${schema.description.text or ""}

    *Type:* ${schema.type.description}
  ''

  # (builtins.toString schema.example)
  # (builtins.toString schema.default)
else let attrs = (builtins.filter (x: x != "_nixAnalyzerLoc") (pkgs.lib.attrNames schema));
  options = builtins.filter (x: schema.${x} ? _type) attrs;
  submodules = builtins.filter (x: !(schema.${x} ? _type)) attrs;
in
  ''
    module `${builtins.concatStringsSep "." schema._nixAnalyzerLoc}`
  ${builtins.concatStringsSep "\n\n" (map (x: "#### " + (getDoc schema.${x})) options)}

  ${builtins.concatStringsSep "\n\n" ((map (x: "#### module `${builtins.concatStringsSep "." (schema._nixAnalyzerLoc ++ [x])}`") submodules))}
  ''); in "### " + (getDoc schema)

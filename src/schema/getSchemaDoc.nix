{
  schema,
  pkgs,
}: let
  getDoc = schema: (
    if schema ? _type
    then ''
      option `${builtins.concatStringsSep "." schema._nixAnalyzerLoc}`
      ${schema.description.text or ""}

      *Type:* ${schema.type.description}
    ''
    else let
      subschemas = (import ./getAttrSubschemas.nix) {
        parent = schema;
        inherit pkgs;
      };
      attrs = builtins.filter (x: !builtins.elem x ["_nixAnalyzerLoc" "_module"]) (pkgs.lib.attrNames subschemas);
      options = builtins.filter (x: subschemas.${x} ? _type) attrs;
      submodules = builtins.filter (x: !(subschemas.${x} ? _type)) attrs;
    in ''
        module `${builtins.concatStringsSep "." schema._nixAnalyzerLoc}`
      ${builtins.concatStringsSep "\n\n" (map (x: "#### " + (getDoc subschemas.${x})) options)}

      ${builtins.concatStringsSep "\n\n" (map (x: "#### module `${builtins.concatStringsSep "." (subschemas._nixAnalyzerLoc ++ [x])}`") submodules)}
    ''
  );
in
  "### " + (getDoc schema)

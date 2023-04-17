{
  schema,
  pkgs
}:

if schema ? type then
  ''
    ```${builtins.concatStringsSep "." schema.loc}```

    ${schema.description.text}

    *Type:* ${schema.type.description}
  ''

  # (builtins.toString schema.example)
  # (builtins.toString schema.default)
else
  null
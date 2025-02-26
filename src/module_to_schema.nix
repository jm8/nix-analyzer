let
  pkgs = import <nixpkgs> {};
in
  module: let
    convert = {
      option,
      loc ? [],
    }: (
      if (!(option ? _type))
      then
        # This is simply a map to other things
        let
          filtered = pkgs.lib.filterAttrs (k: v: !(pkgs.lib.strings.hasPrefix "_" k)) option;
          converted = builtins.mapAttrs (k: v:
            convert {
              option = v;
              loc = loc ++ [k];
            })
          filtered;

          shortDescription = ''
            ### module `${builtins.concatStringsSep "." loc}`
          '';
          description = shortDescription;
        in {
          properties = converted;
          inherit description shortDescription;
        }
      else if option._type == "option"
      then
        (convertType option.type)
        // {
          description = ''
            ### option `${builtins.concatStringsSep "." loc}`
            ${option.description.text or option.description or ""}

            *Type:* ${option.type.description}
          '';
        }
      else {}
    );
    convertType = type: {};
  in (convert {option = module.options;})

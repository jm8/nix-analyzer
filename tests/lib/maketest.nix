{
  type,
  source,
  expected,
  expectedErrors,
} @ attrs:
with builtins; let
  splitted = split "\\^" source;
  beforeCursor = elemAt splitted 0;
  afterCursor = elemAt splitted 2;
  linesBefore = filter isString (split "\n" beforeCursor);
  lastLine = elemAt linesBefore ((length linesBefore) - 1);
  line = length linesBefore - 1;
  col = stringLength lastLine;
in
  attrs
  // {
    source = replaceStrings ["^"] [""] source;
    inherit line col;
  }

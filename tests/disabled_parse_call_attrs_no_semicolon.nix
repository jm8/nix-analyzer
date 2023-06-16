import ./lib/maketest.nix {
  # this was disabled since the lookaheadBind trick broke other things
  disabled = true;
  type = "parse";
  source = ''
    {
      first = f a abc^
      other.a.b.c = 2;
    }
  '';
  expected = ''
    { first = (f a abc); other = { a = { b = { c = 2; }; }; }; }
  '';
  expectedErrors = ["expected ';', got 'ID' 2:2-2:7"];
  expectedExprPath = ["ExprVar" "ExprCall" "ExprAttrs"];
}

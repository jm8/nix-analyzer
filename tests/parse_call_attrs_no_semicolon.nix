import ./lib/maketest.nix {
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
  expectedErrors = ["expected ';', got 'ID' 2:2-2:8"];
  expectedExprPath = ["ExprVar" "ExprCall" "ExprAttrs"];
}

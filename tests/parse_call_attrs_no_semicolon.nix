import ./lib/maketest.nix {
  type = "parse";
  source = ''
    {
      first = f a abc^
      other.a.b.c = 2;
    }
  '';
  expected = ''
    { first = (f a abc (other).a.b.c); }
  '';
  expectedErrors = ["expected ';', got '=' 2:14-2:16"];
  expectedExprPath = ["ExprVar" "ExprCall" "ExprAttrs"];
}

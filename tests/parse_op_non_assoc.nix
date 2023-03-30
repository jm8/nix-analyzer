# https://github.com/oxalica/nil/blob/main/crates/syntax/test_data/parser/ok/0004-operator.nix
import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    1 < ^2 < 3
  '';
  expected = ''
    (__lessThan (__lessThan 1 2) 3)
  '';
  expectedErrors = [
    "operator < is not associative 0:6-0:7"
  ];
  expectedExprPath = [ "ExprInt" "ExprCall" "ExprCall" ];
}


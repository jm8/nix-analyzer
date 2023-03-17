# https://github.com/oxalica/nil/blob/main/crates/syntax/test_data/parser/ok/0004-operator.nix
import ./lib/maketest.nix
{
  type = "parse";
  source = ''
    1.5 -> 2 || -3 && 4 == 5 || 6^ < 7 || 8 // !9 + 10 * 11 ++ 12 13 ? a // 1
  '';
  expected = ''
    (1.5 -> (((2 || ((__sub 0 3) && (4 == 5))) || (__lessThan 6 7)) || (8 // ((! (9 + (__mul 10 (11 ++ (((12 13)) ? a))))) // 1))))
  '';
  expectedExprPath = [ "ExprInt" "ExprCall" "ExprOpOr" "ExprOpOr" "ExprOpImpl" ];
}

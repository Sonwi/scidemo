#include "sci-utils.hpp"
int main(int argc, char **argv) {
  parse_arg(argc, argv);
  init_global_val();

  // test_split_recon();
  // test_add();
  // test_ele_product();
  // test_inner_sum();
  
  // test_ring_drelu_ideal();
  // test_field_drelu();
  // if(party == ALICE) {
  //   cout << prime_mod << endl;
  //   print_binary(prime_mod);
  //   int i = -10;
  //   print_binary(i);
  // }
  // test_make_positive();
  // test_my_drelu();
  // test_make_positive();
  // uint64_t a = 1;
  // uint64_t b = -1 * a;
  // print_binary(a);
  // print_binary(b);
  // test_fix_point();
  // test_scale_fix();
  test_div_cleartext();
  return 0;
}

#include <iostream>
#include <utils/emp-tool.h>
#include <LinearOT/linear-ot.h>
#include <library_fixed.h>
#include <vector>

using namespace std;
using namespace sci;

int party, port = 32000;
string address = "127.0.0.1";
IOPack *iopack;
OTPack *otpack;
LinearOT *prod;
PRG128 *prg;

int dim = 16;
int bwA = 32;
int bwB = 32;
int bwC = 32;
int bw = 32;

uint64_t maskA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
uint64_t maskB = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
uint64_t maskC = (bwC == 64 ? -1 : ((1ULL << bwC) - 1));

uint64_t* split_integer(int dim, int* nums, int bw, PRG128* prg) {
  uint64_t *x_1 = new uint64_t[dim];
  if(party == ALICE) {
    uint64_t *x_0 = new uint64_t[dim];
    prg->random_data(x_1, sizeof(uint64_t) * dim);
    for(int i = 0; i < dim; ++i) {
        x_1[i] &= maskA;
        x_0[i] = nums[i] - x_1[i];
        x_0[i] &= maskA;
    }
    iopack->io->send_data(x_1, sizeof(uint64_t) * dim);
    return x_0;
  } else if (party == BOB) {
    iopack->io->recv_data(x_1, sizeof(uint64_t) * dim);
    return x_1;
  } else {
    throw "party error";
  }
}

#define print_uint(x, dim)\
    for(int i = 0; i < dim; ++i ) { \
        std::cout << int(x[i]) << " "; \
    } \
    std::cout << std::endl;

#define vec_ptr(a) &(*a.begin())

#define recon_print(x, dim)\
    reconstruct(dim, x, bw);\
    if(party == ALICE)\
      print_uint(x, dim);

//alice get output at x_0
void reconstruct(int dim, uint64_t *x_0, int bw_x) {
  uint64_t mask = (bw_x == 64 ? -1 : ((1ULL << bw_x) - 1));
  if(party == ALICE) {
    uint64_t *x_1 = new uint64_t[dim];
    iopack->io->recv_data(x_1, sizeof(uint64_t) * dim);
    for(int i = 0; i < dim; ++i) {
      x_0[i] += x_1[i];
      x_0[i] &= mask;
    }
  }else if(party == BOB) {
      iopack->io->send_data(x_0, sizeof(uint64_t) * dim);
  } else {
    throw "party error";
  }
}

void parse_arg(int argc, char** argv) {
  ArgMapping amap;
  amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("ip", address, "IP Address of server (ALICE)");

  amap.parse(argc, argv);
}

void init_global_val() {
  iopack = new IOPack(party, port, address);
  otpack = new OTPack(iopack, party);

  prod = new LinearOT(party, iopack, otpack);
  prg = new PRG128();
}


void test_split_recon() {
  int *nums = new int[16];
  for(int i = 0; i < dim; ++i) {
    nums[i] = -i;
  }
  uint64_t *x_share = split_integer(dim, nums, 32, prg);
  
  reconstruct(dim, x_share, 32);

  if(party == ALICE) {
    print_uint(x_share, dim);
  }
}

std::pair<uint64_t*, uint64_t*> prepare_data(vector<int>&& nums_a, vector<int>&& nums_b) {
  uint64_t* share_a = split_integer(nums_a.size(), vec_ptr(nums_a), bw, prg);
  uint64_t* share_b = split_integer(nums_b.size(), vec_ptr(nums_b), bw, prg);
  return std::pair<uint64_t*, uint64_t*>(share_a, share_b);
}

void test_add() {
  auto u_pair = prepare_data({-1,-2,-3,-4}, {-100,-200,-300,-400});

  uint64_t *c = new uint64_t[4];
  for(int i = 0; i < 4; ++i) {
    c[i] = u_pair.first[i] - u_pair.second[i];
  }
  reconstruct(4, c, bw);
  if(party == ALICE) {
    print_uint(c, 4);
  }
}

void test_ele_product() {
  auto num_pair = prepare_data({-1,-2,-3,-4,-5}, {-1,-2,-3,-4,-5});
  uint64_t *res = new uint64_t[5];

  prod->hadamard_product(5, num_pair.first, num_pair.second, res, bw, bw, bw);

  recon_print(res, 5);
//   reconstruct(5, res, 32);

//   if(party == ALICE)
//     print_uint(res, 5);
}

int main(int argc, char **argv) {
  parse_arg(argc, argv);
  init_global_val();

//   test_split_recon();
//   test_add();
  test_ele_product();
  return 0;
}

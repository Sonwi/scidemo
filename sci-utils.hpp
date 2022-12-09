#include <iostream>
#include <utils/emp-tool.h>
#include <LinearOT/linear-ot.h>
#include <library_fixed.h>
#include <vector>
#include <bitset>
#include <NonLinear/relu-ring.h>
#include <NonLinear/drelu-field.h>
#include <BuildingBlocks/aux-protocols.h>

using namespace std;
using namespace sci;

int party, port = 32000;
string address = "127.0.0.1";
IOPack *my_iopack;
OTPack *my_otpack;
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
  uint64_t mask = (bw == 64 ? -1 : ((1ULL << bw) - 1));
  uint64_t *x_1 = new uint64_t[dim];
  if(party == ALICE) {
    uint64_t *x_0 = new uint64_t[dim];
    prg->random_data(x_1, sizeof(uint64_t) * dim);
    for(int i = 0; i < dim; ++i) {
        x_1[i] &= mask;
        x_0[i] = nums[i] - x_1[i];
        x_0[i] &= mask;
    }
    my_iopack->io->send_data(x_1, sizeof(uint64_t) * dim);
    return x_0;
  } else if (party == BOB) {
    my_iopack->io->recv_data(x_1, sizeof(uint64_t) * dim);
    return x_1;
  } else {
    throw "party error";
  }
}

template<typename T>
inline void print_binary(T num) {
  std::cout << (bitset<sizeof(T) * 8>)num << std::endl;
}

#define print_uint(x, dim)\
    for(int i = 0; i < dim; ++i ) { \
        std::cout << int64_t(signed_val(x[i], bw)) << " "; \
        std::cout << (bitset<sizeof(x[i]) * 8>)signed_val(x[i], bw) << std::endl;\
    } \
    std::cout << std::endl;

#define vec_ptr(a) &(*a.begin())

#define recon_print(x, dim, bw)\
    reconstruct(dim, x, bw);\
    if(party == ALICE)\
      print_uint(x, dim);

//alice get output at x_0
template<typename T>
void reconstruct(int dim, T *x_0, int bw_x) {
  T mask = (bw_x == 64 ? -1 : ((1ULL << bw_x) - 1));
  if(party == ALICE) {
    T *x_1 = new T[dim];
    my_iopack->io->recv_data(x_1, sizeof(T) * dim);
    for(int i = 0; i < dim; ++i) {
      x_0[i] += x_1[i];
      x_0[i] &= mask;
    }
  }else if(party == BOB) {
      my_iopack->io->send_data(x_0, sizeof(T) * dim);
  } else {
    throw "party error";
  }
}

void inner_sum(int dim, uint64_t *x_share, uint64_t *out, int bw_x) {
  uint64_t mask = (bw_x == 64 ? -1 : ((1ULL << bw_x) - 1));
  for(int i = 0; i < dim; ++i) {
    out[0] += x_share[i];
    out[0] &= mask;
  }
}

uint64_t* prepare_data(vector<int>&& nums_a) {
  int*a_ptr = new int[nums_a.size()];
  memcpy(a_ptr, vec_ptr(nums_a), sizeof(int) * nums_a.size());
  uint64_t* share_a = split_integer(nums_a.size(), a_ptr, bw, prg);
  delete[] a_ptr;
  return share_a;
}

std::pair<uint64_t*, uint64_t*> prepare_data(vector<int>&& nums_a, vector<int>&& nums_b)  {
  uint64_t* share_a = prepare_data(std::move(nums_a));
  uint64_t* share_b = prepare_data(std::move(nums_b));
  return std::pair<uint64_t*, uint64_t*>(share_a, share_b);
}

void test_inner_sum() {
  // vector<int> nums{-1,-2,-3,-4,-5};
  // uint64_t *x_share = split_integer(nums.size(), vec_ptr(nums), bw, prg);inner
  auto num_pair = prepare_data({-1,-2,-3,-4,-5}, {1,3,2,4,5});
  uint64_t *out = new uint64_t(0);
  inner_sum(5, num_pair.first, out, bw);
  recon_print(out, 1, bw);
  *out = 0;
  inner_sum(5, num_pair.second, out, bw);
  recon_print(out, 1, bw);
}

void test_ring_drelu_ideal() {
  ReLURingProtocol<uint64_t> *relu_protocol = new ReLURingProtocol<uint64_t>(party, RING, my_iopack, bw, 4, my_otpack);
  uint64_t* share_a = prepare_data({-1,1,-2,2});
  if(party == ALICE) {
    uint64_t* share_b = new uint64_t[4];
    my_iopack->io->recv_data(share_b, sizeof(uint64_t) * 4);
    uint8_t* res = new uint8_t[4];
    relu_protocol->drelu_ring_ideal_func(res, share_a, share_b, 4);
  }else if(party == BOB) {
    my_iopack->io->send_data(share_a, sizeof(uint64_t) * 4);
  } else {
    throw "party error";
  }
}

void test_field_drelu() {
  DReLUFieldProtocol* drelu_protocol = new DReLUFieldProtocol(party, 32, 4, prime_mod, my_iopack, my_otpack);

  uint64_t *x_share = prepare_data({-2});

  uint8_t *res = new uint8_t[1];

  drelu_protocol->compute_drelu(res, x_share, 1);

  // print_binary(res[0]);
  recon_print(res, 1, 1);
}

void parse_arg(int argc, char** argv) {
  ArgMapping amap;
  amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("ip", address, "IP Address of server (ALICE)");

  amap.parse(argc, argv);
}

void init_global_val() {
  my_iopack = new IOPack(party, port, address);
  my_otpack = new OTPack(my_iopack, party);

  prod = new LinearOT(party, my_iopack, my_otpack);
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
  auto num_pair = prepare_data({-1,-2,-3,-4,-5}, {-1,-2,-3,-4,5});
  uint64_t *res = new uint64_t[5];

  prod->hadamard_product(5, num_pair.first, num_pair.second, res, bw, bw, bw);

  recon_print(res, 5, bw);
//   reconstruct(5, res, 32);

//   if(party == ALICE)
//     print_uint(res, 5);
}

int num_threads = 1;
int32_t bitlength = 32;
extern uint64_t prime_mod;

// void test_make_positive() {
//   AuxProtocols* aux = new AuxProtocols(party, my_iopack, my_otpack);

//   uint64_t *x_share = prepare_data({-1,1,-2,2});
//   uint8_t *res = new uint8_t[4];

//   aux->MSB(x_share, res, 4, bw);

// //   uint64_t *x_po = new uint64_t[4];
// //   aux->make_positive(x_share, res, x_po, 4);

//   recon_print(res, 4, 1);

//   delete aux;
//   delete[] res;
// }

void my_drelu(AuxProtocols* aux, uint64_t *x, uint8_t *res, int size, int bw) {
  aux->MSB(x, res, size, bw);
  uint8_t b = (party == ALICE)? 0 : 1;
  for(int i = 0; i < size; ++i) {
    res[i] = res[i] ^ b;
  }
}

void test_my_drelu() {
  AuxProtocols *aux = new AuxProtocols(party, my_iopack, my_otpack);

  uint64_t *x = prepare_data({-10, 10, -100, 100});

  uint8_t *res = new uint8_t[4];
  my_drelu(aux, x, res, 4, bw);

  recon_print(res, 4, 1);
}

void make_positive(AuxProtocols *aux, uint64_t *x, uint64_t *positive_x, int size, int bw) {
  uint64_t mask = (bw == 64 ? -1 : ((1ULL << bw) - 1));
  uint8_t *drelu_res = new uint8_t[size];
  my_drelu(aux, x, drelu_res, size, bw);
  uint64_t *op_x = new uint64_t[size];
  uint8_t *op_relu = new uint8_t[size];
  for(int i = 0; i < size; i++) {
    op_x[i] = (-x[i]) & mask;
    op_relu[i] = drelu_res[i] ^ (party == ALICE?0:1);
  }

  uint64_t *out1 = new uint64_t[size];
  uint64_t *out2 = new uint64_t[size];
  aux->multiplexer(drelu_res, x, out1, size, bw, bw);
  aux->multiplexer(op_relu, op_x, out2, size, bw, bw);

  for(int i = 0; i < size; ++i) {
    positive_x[i] = out1[i] + out2[i];
    positive_x[i] &= mask;
  }

  delete[] out1;
  delete[] out2;
  delete[] drelu_res;
  delete[] op_x;
  delete[] op_relu;
}

void test_make_positive() {
  AuxProtocols* aux = new AuxProtocols(party, my_iopack, my_otpack);

  uint64_t *x = prepare_data({-100000,-2,-3,-4,5, 100000});

  uint64_t *y = new uint64_t[6];
  make_positive(aux, x, y, 6, bw);

  recon_print(y, 6, bw);
}
#include <iostream>
#include <utils/emp-tool.h>
#include <LinearOT/linear-ot.h>
#include <library_fixed.h>

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

uint64_t maskA = (bwA == 64 ? -1 : ((1ULL << bwA) - 1));
uint64_t maskB = (bwB == 64 ? -1 : ((1ULL << bwB) - 1));
uint64_t maskC = (bwC == 64 ? -1 : ((1ULL << bwC) - 1));

void test_hadamard_product(uint64_t *inA, uint64_t *inB,
                           bool signed_arithmetic = true) {
  uint64_t *outC = new uint64_t[dim];

  prod->hadamard_product(dim, inA, inB, outC, bwA, bwB, bwC, signed_arithmetic);

  if (party == ALICE) {
    iopack->io->send_data(inA, dim * sizeof(uint64_t));
    iopack->io->send_data(inB, dim * sizeof(uint64_t));
    iopack->io->send_data(outC, dim * sizeof(uint64_t));
  } else { // party == BOB
    uint64_t *inA0 = new uint64_t[dim];
    uint64_t *inB0 = new uint64_t[dim];
    uint64_t *outC0 = new uint64_t[dim];
    iopack->io->recv_data(inA0, dim * sizeof(uint64_t));
    iopack->io->recv_data(inB0, dim * sizeof(uint64_t));
    iopack->io->recv_data(outC0, dim * sizeof(uint64_t));

    for (int i = 0; i < dim; i++) {
      if (signed_arithmetic) {
        assert(signed_val(outC[i] + outC0[i], bwC) ==
               (signed_val(signed_val(inA[i] + inA0[i], bwA) *
                               signed_val(inB[i] + inB0[i], bwB),
                           bwC)));
      } else {
        assert(unsigned_val(outC[i] + outC0[i], bwC) ==
               (unsigned_val(unsigned_val(inA[i] + inA0[i], bwA) *
                                 unsigned_val(inB[i] + inB0[i], bwB),
                             bwC)));
      }
    }
    if (signed_arithmetic)
      cout << "SMult Tests Passed" << endl;
    else
      cout << "UMult Tests Passed" << endl;

    delete[] inA0;
    delete[] inB0;
    delete[] outC0;
  }

  delete[] outC;
}

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

void test_add() {
  int mat_a[] = {-1,-2,-3,-100};
  int mat_b[] = {-1,-2,-3,-100};
  uint64_t *a_share = split_integer(4, mat_a, 32, prg);
  uint64_t *b_share = split_integer(4, mat_b, 32, prg);

  uint64_t *c = new uint64_t[4];
  for(int i = 0; i < 4; ++i) {
    c[i] = a_share[i] - b_share[i];
  }
  reconstruct(4, c, 32);
  if(party == ALICE) {
    print_uint(c, 4);
  }
}

int main(int argc, char **argv) {
  parse_arg(argc, argv);
  init_global_val();

  test_split_recon();
  test_add();
  return 0;
}

int main2(int argc, char **argv) {
  ArgMapping amap;
  amap.arg("r", party, "Role of party: ALICE = 1; BOB = 2");
  amap.arg("p", port, "Port Number");
  amap.arg("ip", address, "IP Address of server (ALICE)");

  amap.parse(argc, argv);

  iopack = new IOPack(party, port, address);
  otpack = new OTPack(iopack, party);

  prod = new LinearOT(party, iopack, otpack);

  PRG128 prg;

  uint64_t *inA = new uint64_t[dim];
  uint64_t *inB = new uint64_t[dim];

  prg.random_data(inA, dim * sizeof(uint64_t));
  prg.random_data(inB, dim * sizeof(uint64_t));

  for (int i = 0; i < dim; i++) {
    inA[i] &= maskA;
    inB[i] &= maskB;
  }

  test_hadamard_product(inA, inB, false);
  test_hadamard_product(inA, inB, true);

  delete[] inA;
  delete[] inB;
  delete prod;
  return 0;
}

#include "../src/thvm.c"
#include "test.h"
#define L_OP 1u
#define L_TILDE 2u
#define L_ONE 3u
#define L_BAR 4u
static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term bin(u32 lab, Term x, Term y) { Term c[2]={x,y}; return term_new_ctr(lab, c, 2); }
static Term un(u32 lab, Term x) { return term_new_ctr(lab, &x, 1); }
static Term v(u32 i) { return term_new_fvr(i); }

static void run(const char *name, int axset) {
  Term x = v(0), y = v(1), z = v(2);
  Term e = un(L_TILDE, k(L_ONE));
  static const u32 W[8] = {1,1,1,1,1,1,1,1};
  static const u32 P[8] = {0,2,3,4,5,6,7,8};
  KboConfig kbo = { .weights=W, .precedence=P, .n_labels=8, .var_weight=1 };
  thvm_atp_set_ac_mask(1ull << L_OP);
  AtpState *s = thvm_atp_init(&kbo, 256);
  // axset bits: 1=comm 2=assoc 4=identity 8=inverse
  if (axset & 1) thvm_atp_add_equation(s, bin(L_OP, x, y), bin(L_OP, y, x));
  if (axset & 2) thvm_atp_add_equation(s, bin(L_OP, bin(L_OP, x, y), z),
                                          bin(L_OP, x, bin(L_OP, y, z)));
  if (axset & 4) thvm_atp_add_equation(s, bin(L_OP, e, x), x);
  if (axset & 8) thvm_atp_add_equation(s, bin(L_OP, un(L_BAR, x), x), e);
  AtpStatus st = ATP_RUNNING;
  for (u32 i = 0; i < 256u; i++) {
    st = thvm_atp_step(s);
    if (st != ATP_RUNNING) break;
  }
  printf("== %s (axset=0x%x): n_rules=%u status=%d ==\n", name, axset, s->n_rules, st);
  thvm_atp_free(s);
  thvm_atp_set_ac_mask(0);
}

int main(void) {
  thvm_init();
  run("inverse-only", 8);
  run("inverse+comm", 9);
  run("inverse+assoc", 10);
  run("inverse+identity", 12);
  run("inverse+comm+identity", 13);
  thvm_free();
  return 0;
}

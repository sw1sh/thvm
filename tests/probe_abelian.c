#include "../src/thvm.c"
#define L_F 1u
#define L_E 2u
#define L_A 3u
#define L_B 4u
#define L_C 5u
#define L_I 6u
static Term k(u32 lab) { return term_new_ctr(lab, NULL, 0); }
static Term bin(u32 lab, Term x, Term y) { Term c[2]={x,y}; return term_new_ctr(lab, c, 2); }
static Term un(u32 lab, Term x) { return term_new_ctr(lab, &x, 1); }
static Term v(u32 i) { return term_new_fvr(i); }
int main(void) {
  thvm_init();
  Term x = v(0), y = v(1), z = v(2);
  Term a = k(L_A), b = k(L_B);
  Term e = k(L_E);
  Term ix = un(L_I, x);
  static const u32 W[8] = {1,1,1,1,1,1,1,1};
  static const u32 P[8] = {0,1,0,2,5,4,3,0};
  KboConfig kbo = { .weights=W, .precedence=P, .n_labels=8, .var_weight=1 };
  thvm_atp_set_ac_mask(1ull << L_F);
  AtpState *s = thvm_atp_init(&kbo, 8192);
  thvm_atp_set_use_unfailing_cp(s, 1);
  thvm_atp_add_equation(s, bin(L_F, x, y), bin(L_F, y, x));
  thvm_atp_add_equation(s, bin(L_F, bin(L_F, x, y), z), bin(L_F, x, bin(L_F, y, z)));
  thvm_atp_add_equation(s, bin(L_F, e, x), x);
  thvm_atp_add_equation(s, bin(L_F, ix, x), e);
  Term ia = un(L_I, a);
  Term ib = un(L_I, b);
  thvm_atp_set_goal(s, un(L_I, bin(L_F, a, b)), bin(L_F, ib, ia));
  for (u32 i = 0; i < 256; i++) {
    AtpStatus st = thvm_atp_step(s);
    if (st != ATP_RUNNING) {
      printf("EXIT iters=%u n_rules=%u status=%d\n", i+1, s->n_rules, st);
      break;
    }
  }
  (void)ia; (void)ib;
  thvm_atp_set_ac_mask(0);
  thvm_atp_free(s);
  thvm_free();
  return 0;
}

// probe_lpo_wm.c -- feed wmcli's first oriented andassoc rules to
// thvm_lpo, report verdicts.  GT means thvm agrees with wmcli's
// orientation; UN/LT means thvm's LPO is more conservative and would
// have left the rule unoriented (the source of the 286/416-unorient
// vs wmcli's ~100/1673 gap on AndAssoc).
//
// Precedence matches WM's andassoc.pr ORDERING block (p > q > r > nand):
//   p (label 2) = rank 4
//   q (label 3) = rank 3
//   r (label 4) = rank 2
//   nand (label 1) = rank 1

#include "../src/thvm.c"
#include <stdio.h>

#define L_NAND 1u
#define L_P    2u
#define L_Q    3u
#define L_R    4u

static u32 lpo_prec[7] = { 0u, 1u, 4u, 3u, 2u, 0u, 0u };
static const LpoConfig LPO_CFG = { .precedence = lpo_prec, .n_labels = 7u };

static Term fv(u32 id) { return term_new_fvr(id); }
static Term nand2(Term x, Term y) { Term c[2] = {x, y}; return term_new_ctr(L_NAND, c, 2); }
static Term p_(void) { return term_new_ctr(L_P, NULL, 0); }
static Term q_(void) { return term_new_ctr(L_Q, NULL, 0); }
static Term r_(void) { return term_new_ctr(L_R, NULL, 0); }

static const char *verdict_name(LpoCmp c) {
  switch (c) {
    case LPO_EQ: return "EQ";
    case LPO_GT: return "GT";
    case LPO_LT: return "LT";
    case LPO_UN: return "UN";
    default:     return "??";
  }
}

static void probe(const char *name, Term lhs, Term rhs) {
  LpoCmp c = thvm_lpo(lhs, rhs, &LPO_CFG);
  printf("%-32s  thvm_lpo = %s  (wmcli said GT)%s\n",
         name, verdict_name(c),
         (c == LPO_GT) ? "" : "  *** DIVERGENCE ***");
}

int main(void) {
  thvm_init();
  Term x1 = fv(0), x2 = fv(1), x3 = fv(2), x4 = fv(3);

  // ###R 1: nand(nand(nand(x1,x2),x3),nand(x1,nand(nand(x1,x3),x1))) -> x3
  {
    Term lhs = nand2(nand2(nand2(x1, x2), x3),
                     nand2(x1, nand2(nand2(x1, x3), x1)));
    probe("R1: ...->x3", lhs, x3);
  }

  // ###R 2: nand(nand(nand(nand(x1,nand(nand(x1,x2),x1)),x3),x2),
  //              nand(nand(x1,nand(nand(x1,x2),x1)),x2)) -> x2
  {
    Term inner = nand2(x1, nand2(nand2(x1, x2), x1));
    Term lhs = nand2(nand2(nand2(inner, x3), x2),
                     nand2(nand2(x1, nand2(nand2(x1, x2), x1)), x2));
    probe("R2: ...->x2", lhs, x2);
  }

  // ###R 3: nand(nand(x1,x2),
  //              nand(nand(nand(x3,x4),x1),
  //                   nand(nand(nand(nand(x3,x4),x1),x2),
  //                        nand(nand(x3,x4),x1)))) -> x2
  {
    Term a = nand2(nand2(x3, x4), x1);
    Term lhs = nand2(nand2(x1, x2),
                     nand2(a, nand2(nand2(a, x2), a)));
    probe("R3: ...->x2", lhs, x2);
  }

  // ###R 5: nand(nand(nand(x1,nand(nand(x1,x2),x1)),x3),
  //              nand(x2,nand(nand(x2,x3),x2))) -> x3
  {
    Term lhs = nand2(nand2(nand2(x1, nand2(nand2(x1, x2), x1)), x3),
                     nand2(x2, nand2(nand2(x2, x3), x2)));
    probe("R5: ...->x3", lhs, x3);
  }

  // ###R 12: nand(x1,nand(x1,nand(nand(x1,nand(x2,nand(nand(x2,x1),x2))),x1)))
  //          -> nand(x2,nand(nand(x2,x1),x2))
  {
    Term rhs = nand2(x2, nand2(nand2(x2, x1), x2));
    Term lhs = nand2(x1, nand2(x1, nand2(nand2(x1, rhs), x1)));
    probe("R12: ...->nand(x2,...)", lhs, rhs);
  }

  // ###R 13: nand(x1,nand(x1,nand(nand(x1,nand(nand(x2,nand(nand(x2,x1),x2)),x1)),x1)))
  //          -> nand(nand(x2,nand(nand(x2,x1),x2)),x1)
  {
    Term inner = nand2(x2, nand2(nand2(x2, x1), x2));
    Term rhs = nand2(inner, x1);
    Term lhs = nand2(x1, nand2(x1, nand2(nand2(x1, nand2(inner, x1)), x1)));
    probe("R13: ...->nand(...,x1)", lhs, rhs);
  }

  // ###R 29 (the CRACKING rule precursor, R#1672 internal id):
  //   nand(nand(x1,nand(x1,nand(nand(x1,x1),x1))),x1) -> nand(x1,nand(nand(x1,x1),x1))
  {
    Term rhs = nand2(x1, nand2(nand2(x1, x1), x1));
    Term lhs = nand2(nand2(x1, nand2(x1, rhs)), x1);
    probe("R29(crack): ...->nand(x1,...)", lhs, rhs);
  }

  // ###R 31: nand(nand(x1,nand(nand(x1,x1),x1)),
  //               nand(x1,nand(nand(x1,x1),x1))) -> x1
  {
    Term a = nand2(x1, nand2(nand2(x1, x1), x1));
    Term lhs = nand2(a, a);
    probe("R31: nand(a,a)->x1", lhs, x1);
  }

  return 0;
}

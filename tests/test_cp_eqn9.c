// test_cp_eqn9.c - decisive Step 1 probe for the soa pick-125 divergence.
//
// WM forms eqn-9 = `(x.x).z # z.(x.z)` EARLIEST at w2=372 from:
//   RULE9 = (x.(y.(y.y))).(z.(z.z)) = x
//   EQ2   = ((a.a).b).((c.c).b) = (b.(a.c)).(b.(a.c))     (soa 3rd axiom, reoriented)
// This probe constructs those EXACT formation-time terms and enumerates
// ALL critical pairs between them (both directions, both faces of the
// unorientable EQ2, all non-variable positions) to test whether thvm's
// overlap enumeration can produce eqn-9 at all.

#include "../src/thvm.c"
#include "test.h"

#define C3 3u  // opCenterdot (binary Sheffer/NAND)

static Term dot(Term a, Term b) { Term cs[2] = {a, b}; return term_new_ctr(C3, cs, 2); }
#define V(i) mk_v((u32)(i))

// RULE9 lhs/rhs: (x.(y.(y.y))).(z.(z.z)) = x   [x=0 y=1 z=2]
static Term rule9_lhs(void) {
  return dot(dot(V(0), dot(V(1), dot(V(1), V(1)))),
             dot(V(2), dot(V(2), V(2))));
}
static Term rule9_rhs(void) { return V(0); }

// EQ2 lhs/rhs: ((a.a).b).((c.c).b) = (b.(a.c)).(b.(a.c))   [a=0 b=1 c=2]
static Term eq2_lhs(void) {
  return dot(dot(dot(V(0), V(0)), V(1)),
             dot(dot(V(2), V(2)), V(1)));
}
static Term eq2_rhs(void) {
  Term inner = dot(V(1), dot(V(0), V(2)));
  return dot(inner, inner);
}

// Canonical alpha-renumber a CP side-pair (both sides share var space),
// then test structural equality against a reference pair (also renumbered).
static u8 same_eqn(Term al, Term ar, Term bl, Term br) {
  Term al2 = al, ar2 = ar, bl2 = bl, br2 = br;
  thvm_normalize_vars(&al2, &ar2);
  thvm_normalize_vars(&bl2, &br2);
  return (al2 == bl2 && ar2 == br2);
}

// eqn-9 in both stored orientations: `(x.x).z = z.(x.z)`.
static u8 is_eqn9(Term l, Term r) {
  // form 1: lhs=(C3 (C3 x x) z) rhs=(C3 z (C3 x z))
  Term e9l = dot(dot(V(0), V(0)), V(1));     // (x.x).z   x=0 z=1
  Term e9r = dot(V(1), dot(V(0), V(1)));     // z.(x.z)
  if (same_eqn(l, r, e9l, e9r)) return 1;
  if (same_eqn(l, r, e9r, e9l)) return 1;
  return 0;
}

static void print_cp(const char *tag, u32 idx, const CriticalPair *cp) {
  fprintf(stdout, "  [%s #%u] pos_len=%u combo=%u  lhs=", tag, idx, cp->pos_len, cp->combo);
  cp_dbg_print_term(stdout, cp->lhs);
  fprintf(stdout, "  rhs=");
  cp_dbg_print_term(stdout, cp->rhs);
  if (is_eqn9(cp->lhs, cp->rhs)) fprintf(stdout, "   <<<<< EQN-9");
  fputc('\n', stdout);
}

// Enumerate CPs overlapping (lj->rj) into li (outer), with lj/rj renamed
// apart.  Returns count of eqn-9 hits found.
static u32 enum_dir(const char *tag, Term li, Term ri, Term lj_raw, Term rj_raw,
                    CriticalPair *cps, u32 cap) {
  Term lj = thvm_rename_vars(lj_raw, CP_RENAME_OFFSET);
  Term rj = thvm_rename_vars(rj_raw, CP_RENAME_OFFSET);
  u32 n = thvm_critical_pairs_pair(li, ri, lj, rj, cps, cap, 0);
  u32 hits = 0;
  fprintf(stdout, "%s : %u CPs\n", tag, n);
  for (u32 i = 0; i < n; i++) {
    print_cp(tag, i, &cps[i]);
    if (is_eqn9(cps[i].lhs, cps[i].rhs)) hits++;
  }
  return hits;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("cp/eqn9-from-WM-exact-terms");
  {
    Term r9l = rule9_lhs(), r9r = rule9_rhs();
    Term e2l = eq2_lhs(), e2r = eq2_rhs();

    fprintf(stdout, "RULE9 lhs="); cp_dbg_print_term(stdout, r9l);
    fprintf(stdout, " rhs="); cp_dbg_print_term(stdout, r9r); fputc('\n', stdout);
    fprintf(stdout, "EQ2   lhs="); cp_dbg_print_term(stdout, e2l);
    fprintf(stdout, " rhs="); cp_dbg_print_term(stdout, e2r); fputc('\n', stdout);
    fprintf(stdout, "TARGET eqn-9 = (C3 (C3 x x) z) = (C3 z (C3 x z))\n\n");

    CriticalPair cps[256] = {{0}};
    u32 hits = 0;

    // Direction A: RULE9 is outer (li), overlap EQ2 (both faces) into it.
    hits += enum_dir("A1 RULE9<-EQ2.fwd", r9l, r9r, e2l, e2r, cps, 256);
    hits += enum_dir("A2 RULE9<-EQ2.rev", r9l, r9r, e2r, e2l, cps, 256);

    // Direction B: EQ2 (both faces) is outer (li), overlap RULE9 into it.
    hits += enum_dir("B1 EQ2.fwd<-RULE9", e2l, e2r, r9l, r9r, cps, 256);
    hits += enum_dir("B2 EQ2.rev<-RULE9", e2r, e2l, r9l, r9r, cps, 256);

    // Also self-overlap each (completeness; should not yield eqn-9 but
    // print for the record).
    hits += enum_dir("S1 RULE9<-RULE9", r9l, r9r, r9l, r9r, cps, 256);

    fprintf(stdout, "\nTOTAL eqn-9 hits across all directions/faces: %u\n", hits);
    CHECK(hits >= 1u);  // the decisive assertion
  }

  thvm_free();
  TEST_REPORT();
}

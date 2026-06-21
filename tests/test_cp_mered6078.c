// test_cp_mered6078.c - decisive probe for Meredith AndAssoc @6078.
//
// WM forms the @6078-selected target T = (x.x).y # y.(y.x) at FIFO age
// w2=23993 from RELIABLE parents (instrumented ELProver WM_CLASSDUMP):
//   rule126 = x.(y.((z.x).(w.z))) -> (y.z).x        [4 vars -- NOT the 3-var
//             term the prior meredith_6078_diagnosis unit test used; that was
//             an ElternNr-reuse artifact]
//   rule36  = x.((x.y).(y.z)) -> y.x                 [3 vars]
// This probe builds those EXACT formation-time terms and enumerates ALL
// critical pairs (both directions, every non-variable position) to test
// whether thvm's overlap enumeration produces T from the reliable pair.

#include "../src/thvm.c"
#include "test.h"

#define C3 3u  // opCenterdot

static Term dot(Term a, Term b) { Term cs[2] = {a, b}; return term_new_ctr(C3, cs, 2); }
#define V(i) mk_v((u32)(i))

// rule126 lhs/rhs: x.(y.((z.x).(w.z))) = (y.z).x   [x=0 y=1 z=2 w=3]
static Term r126_lhs(void) {
  return dot(V(0), dot(V(1), dot(dot(V(2), V(0)), dot(V(3), V(2)))));
}
static Term r126_rhs(void) { return dot(dot(V(1), V(2)), V(0)); }

// rule36 lhs/rhs: x.((x.y).(y.z)) = y.x   [x=0 y=1 z=2]
static Term r36_lhs(void) {
  return dot(V(0), dot(dot(V(0), V(1)), dot(V(1), V(2))));
}
static Term r36_rhs(void) { return dot(V(1), V(0)); }

static u8 same_eqn(Term al, Term ar, Term bl, Term br) {
  // normalize_vars returns non-hash-consed scratch terms, so two structurally
  // identical normal forms get DIFFERENT handles -- compare by structural hash.
  Term al2 = al, ar2 = ar, bl2 = bl, br2 = br;
  thvm_normalize_vars(&al2, &ar2);
  thvm_normalize_vars(&bl2, &br2);
  return atp_term_struct_hash(al2) == atp_term_struct_hash(bl2)
      && atp_term_struct_hash(ar2) == atp_term_struct_hash(br2);
}

// T = (x.x).y = y.(y.x)
static u8 is_T(Term l, Term r) {
  Term tl = dot(dot(V(0), V(0)), V(1));   // (x.x).y
  Term tr = dot(V(1), dot(V(1), V(0)));   // y.(y.x)
  if (same_eqn(l, r, tl, tr)) return 1;
  if (same_eqn(l, r, tr, tl)) return 1;
  return 0;
}

static u32 enum_dir(const char *tag, Term li, Term ri, Term lj_raw, Term rj_raw,
                    CriticalPair *cps, u32 cap) {
  Term lj = thvm_rename_vars(lj_raw, CP_RENAME_OFFSET);
  Term rj = thvm_rename_vars(rj_raw, CP_RENAME_OFFSET);
  u32 n = thvm_critical_pairs_pair(li, ri, lj, rj, cps, cap, 0);
  u32 hits = 0;
  fprintf(stdout, "%s : %u CPs\n", tag, n);
  for (u32 i = 0; i < n; i++) {
    Term nl = cps[i].lhs, nr = cps[i].rhs;
    thvm_normalize_vars(&nl, &nr);
    fprintf(stdout, "  [%s #%u] pl=%u norm: ", tag, i, cps[i].pos_len);
    cp_dbg_print_term(stdout, nl);
    fprintf(stdout, " # ");
    cp_dbg_print_term(stdout, nr);
    if (is_T(cps[i].lhs, cps[i].rhs)) { hits++; fprintf(stdout, "  <<<<< T"); }
    fputc('\n', stdout);
  }
  return hits;
}

int main(void) {
  thvm_init();
  TEST_BEGIN("cp/mered6078-from-WM-reliable-terms");
  {
    Term a = r126_lhs(), b = r126_rhs();
    Term c = r36_lhs(),  d = r36_rhs();
    CriticalPair cps[512] = {{0}};
    u32 hits = 0;
    hits += enum_dir("A1 r126<-r36.fwd", a, b, c, d, cps, 512);
    hits += enum_dir("A2 r126<-r36.rev", a, b, d, c, cps, 512);
    hits += enum_dir("B1 r36.fwd<-r126", c, d, a, b, cps, 512);
    hits += enum_dir("B2 r36.rev<-r126", d, c, a, b, cps, 512);
    fprintf(stdout, "\nTOTAL T hits: %u\n", hits);
    CHECK(hits >= 1u);
  }
  thvm_free();
  TEST_REPORT();
}

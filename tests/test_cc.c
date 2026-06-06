// test_cc.c - ground congruence closure (QF_UF) decision procedure.
//
// Unit coverage (reflexivity, transitivity, congruence, the classic
// UNSAT, nested terms) plus a randomized differential against a
// brute-force fixpoint oracle over the same egraph.  Mirrors the
// semantics of WL TSatEUF (wl/THVMLink/Kernel/ATP/SMT.wl).

#include "../src/thvm.c"
#include "test.h"

// === unit fixtures ===

// labels (arbitrary, distinct integer ids)
#define FN_F 1u
#define FN_G 2u
#define FN_H 3u

static void test_reflexivity(void) {
  TEST_BEGIN("reflexivity");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1);
  // no assertions: a is its own class; a != b is SAT
  u32 b = cc_atom(s, 2);
  CHECK(cc_find(s, a) == cc_find(s, a));
  cc_assert_ne(s, a, b);
  CHECK(cc_check(s) == CC_SAT);
  // a self-disequality a != a is immediate UNSAT (a ~ a always)
  cc_assert_ne(s, a, a);
  CHECK(cc_check(s) == CC_UNSAT);
  cc_free(s);
}

static void test_transitivity(void) {
  TEST_BEGIN("transitivity");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2), c = cc_atom(s, 3);
  cc_assert_eq(s, a, b);
  cc_assert_eq(s, b, c);
  CHECK(cc_find(s, a) == cc_find(s, c));   // a = b, b = c => a = c
  cc_assert_ne(s, a, c);
  CHECK(cc_check(s) == CC_UNSAT);
  cc_free(s);
}

static void test_congruence_unary(void) {
  TEST_BEGIN("congruence unary: a=b => f(a)=f(b)");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2);
  u32 fa = cc_app(s, FN_F, &a, 1);
  u32 fb = cc_app(s, FN_F, &b, 1);
  CHECK(cc_find(s, fa) != cc_find(s, fb));  // not yet equal
  cc_assert_eq(s, a, b);
  CHECK(cc_find(s, fa) == cc_find(s, fb));  // congruence fired
  cc_free(s);
}

static void test_congruence_binary(void) {
  TEST_BEGIN("congruence binary: a=c,b=d => f(a,b)=f(c,d)");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2);
  u32 c = cc_atom(s, 3), d = cc_atom(s, 4);
  u32 ab[2] = {a, b}, cd[2] = {c, d};
  u32 fab = cc_app(s, FN_F, ab, 2);
  u32 fcd = cc_app(s, FN_F, cd, 2);
  cc_assert_eq(s, a, c);
  CHECK(cc_find(s, fab) != cc_find(s, fcd));  // only one arg equal
  cc_assert_eq(s, b, d);
  CHECK(cc_find(s, fab) == cc_find(s, fcd));  // both args equal now
  cc_free(s);
}

// The classic EUF UNSAT: a=b, b=c, f(a) != f(c).
static void test_classic_unsat(void) {
  TEST_BEGIN("classic unsat: a=b,b=c,f(a)!=f(c)");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2), c = cc_atom(s, 3);
  u32 fa = cc_app(s, FN_F, &a, 1);
  u32 fc = cc_app(s, FN_F, &c, 1);
  cc_assert_eq(s, a, b);
  cc_assert_eq(s, b, c);
  cc_assert_ne(s, fa, fc);
  CHECK(cc_check(s) == CC_UNSAT);
  cc_free(s);
}

// Same shape but the disequality is over a different pair -> SAT.
static void test_classic_sat(void) {
  TEST_BEGIN("classic sat variant");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2), c = cc_atom(s, 3);
  u32 fa = cc_app(s, FN_F, &a, 1);
  u32 fb = cc_app(s, FN_F, &b, 1);
  (void)fb;
  cc_assert_eq(s, a, b);
  // f(a)!=f(c): a and c are NOT merged, so this is satisfiable
  u32 fc = cc_app(s, FN_F, &c, 1);
  cc_assert_ne(s, fa, fc);
  CHECK(cc_check(s) == CC_SAT);
  cc_free(s);
}

// Nested: g(g(a)) = a, then g(a) related.  Hofmann-style fixpoint.
static void test_nested(void) {
  TEST_BEGIN("nested g(g(a))=a");
  CcState *s = cc_init();
  u32 a  = cc_atom(s, 1);
  u32 ga = cc_app(s, FN_G, &a, 1);
  u32 gga = cc_app(s, FN_G, &ga, 1);
  cc_assert_eq(s, gga, a);
  // now g(g(g(g(a)))) must collapse to a too (g(g(.)) is identity)
  u32 ggga = cc_app(s, FN_G, &gga, 1);
  u32 gggga = cc_app(s, FN_G, &ggga, 1);
  CHECK(cc_find(s, gggga) == cc_find(s, a));
  // and g(g(g(a))) = g(a)
  CHECK(cc_find(s, ggga) == cc_find(s, ga));
  cc_free(s);
}

// Hash-consing: building f(a) twice returns the same node; building
// f(a) and f(b) after a=b returns congruent (but distinct) nodes.
static void test_hashcons(void) {
  TEST_BEGIN("hash-consing");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2);
  u32 fa1 = cc_app(s, FN_F, &a, 1);
  u32 fa2 = cc_app(s, FN_F, &a, 1);
  CHECK(fa1 == fa2);                     // same node, interned
  // after a=b, cc_app(f, b) canonicalizes to f(a)'s class
  cc_assert_eq(s, a, b);
  u32 fb = cc_app(s, FN_F, &b, 1);
  CHECK(cc_find(s, fb) == cc_find(s, fa1));
  cc_free(s);
}

// Deeper congruence cascade: f(f(a)) over a chain a=b=c with an
// f(f(c)) disequality witness.
static void test_deep_cascade(void) {
  TEST_BEGIN("deep cascade f(f(a)) != f(f(c)) with a=b=c");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2), c = cc_atom(s, 3);
  u32 fa  = cc_app(s, FN_F, &a, 1);
  u32 ffa = cc_app(s, FN_F, &fa, 1);
  u32 fc  = cc_app(s, FN_F, &c, 1);
  u32 ffc = cc_app(s, FN_F, &fc, 1);
  cc_assert_eq(s, a, b);
  cc_assert_eq(s, b, c);
  CHECK(cc_find(s, ffa) == cc_find(s, ffc));   // congruence two levels deep
  cc_assert_ne(s, ffa, ffc);
  CHECK(cc_check(s) == CC_UNSAT);
  cc_free(s);
}

// === push/pop round-trip ===
//
// Snapshot the observable state (find-partition over all built nodes +
// the cc_check verdict), push, mutate, pop, and require the snapshot to
// match exactly.  The "observable" state is what any consumer can see
// through cc_find / cc_check; capacity-only internals (sig table growth)
// are deliberately not part of the contract.

#define RT_MAX 64

typedef struct {
  u32 n;
  u32 root[RT_MAX];     // canonical pair-id per node (compressed labels)
  int sat;
} RtSnap;

static void rt_snapshot(CcState *s, u32 *ids, u32 n, RtSnap *snap) {
  snap->n = n;
  // canonicalize roots to dense labels so we compare partitions, not the
  // raw (possibly path-compressed) root node ids.
  u32 label[RT_MAX];
  u32 raw[RT_MAX];
  u32 nlab = 0;
  for (u32 i = 0; i < n; i++) {
    u32 r = cc_find(s, ids[i]);
    u32 li = nlab;
    for (u32 j = 0; j < nlab; j++) if (raw[j] == r) { li = j; break; }
    if (li == nlab) { raw[nlab] = r; nlab++; }
    label[i] = li;
  }
  for (u32 i = 0; i < n; i++) snap->root[i] = label[i];
  snap->sat = (cc_check(s) == CC_SAT);
}

static int rt_eq(const RtSnap *a, const RtSnap *b) {
  if (a->n != b->n || a->sat != b->sat) return 0;
  for (u32 i = 0; i < a->n; i++) if (a->root[i] != b->root[i]) return 0;
  return 1;
}

static void test_push_pop_roundtrip(void) {
  TEST_BEGIN("push/pop observable round-trip");
  CcState *s = cc_init();
  u32 a = cc_atom(s, 1), b = cc_atom(s, 2), c = cc_atom(s, 3);
  u32 fa = cc_app(s, FN_F, &a, 1);
  u32 fb = cc_app(s, FN_F, &b, 1);
  u32 fc = cc_app(s, FN_F, &c, 1);
  u32 ids[6] = {a, b, c, fa, fb, fc};
  cc_assert_eq(s, a, b);            // baseline state before the push

  RtSnap before; rt_snapshot(s, ids, 6, &before);

  // push, then make a pile of mutations: new nodes, unions (firing
  // congruence), and a disequality that flips SAT -> UNSAT.
  u32 mark = cc_push(s);
  u32 d  = cc_atom(s, 4);
  u32 fd = cc_app(s, FN_F, &d, 1);
  cc_assert_eq(s, c, d);           // c=d => f(c)=f(d) by congruence
  cc_assert_eq(s, b, c);           // now a=b=c=d, f(a)=f(b)=f(c)=f(d)
  cc_assert_ne(s, fa, fc);         // f(a) != f(c) but they are congruent
  CHECK(cc_check(s) == CC_UNSAT);
  (void)fd;
  cc_pop(s, mark);

  RtSnap after; rt_snapshot(s, ids, 6, &after);
  CHECK(rt_eq(&before, &after));
  // f(a)=f(b) (since a=b), f(c) distinct, SAT
  CHECK(cc_find(s, fa) == cc_find(s, fb));
  CHECK(cc_find(s, fa) != cc_find(s, fc));
  CHECK(cc_check(s) == CC_SAT);
  cc_free(s);
}

// === brute-force reference oracle ===
//
// An independent O(n^3)-ish fixpoint congruence closure over a flat
// record of every node the test built.  Used to differential-check the
// near-linear DST implementation on random instances.  This duplicates
// nothing from cc/_.c: it is a deliberately naive transitive +
// congruence saturation over a parallel parent array.

#define REF_MAX_NODES 256

typedef struct {
  u32 fn_id;     // CC_NONE for atom
  u32 atom_id;
  u32 nargs;
  u32 args[4];
} RefNode;

typedef struct {
  RefNode node[REF_MAX_NODES];
  u32     n;
  u32     parent[REF_MAX_NODES];
} Ref;

static void ref_init(Ref *r) { r->n = 0; }

static u32 ref_find(Ref *r, u32 x) {
  while (r->parent[x] != x) x = r->parent[x];
  return x;
}

static u32 ref_atom(Ref *r, u32 atom_id) {
  u32 i = r->n++;
  r->node[i].fn_id = CC_NONE;
  r->node[i].atom_id = atom_id;
  r->node[i].nargs = 0;
  r->parent[i] = i;
  return i;
}

static u32 ref_app(Ref *r, u32 fn_id, const u32 *args, u32 nargs) {
  // no hash-consing: structurally identical apps may get distinct ids,
  // which the congruence fixpoint then merges anyway.
  u32 i = r->n++;
  r->node[i].fn_id = fn_id;
  r->node[i].nargs = nargs;
  for (u32 k = 0; k < nargs; k++) r->node[i].args[k] = args[k];
  r->parent[i] = i;
  return i;
}

static void ref_union(Ref *r, u32 a, u32 b) {
  u32 ra = ref_find(r, a), rb = ref_find(r, b);
  if (ra != rb) r->parent[ra] = rb;
}

// saturate transitivity (implicit in union-find) + congruence to fixpoint
static void ref_saturate(Ref *r) {
  int changed = 1;
  while (changed) {
    changed = 0;
    for (u32 i = 0; i < r->n; i++) {
      if (r->node[i].fn_id == CC_NONE) continue;
      for (u32 j = i + 1; j < r->n; j++) {
        if (r->node[j].fn_id != r->node[i].fn_id) continue;
        if (r->node[j].nargs != r->node[i].nargs) continue;
        if (ref_find(r, i) == ref_find(r, j)) continue;
        int all_eq = 1;
        for (u32 k = 0; k < r->node[i].nargs; k++) {
          if (ref_find(r, r->node[i].args[k]) !=
              ref_find(r, r->node[j].args[k])) { all_eq = 0; break; }
        }
        if (all_eq) { ref_union(r, i, j); changed = 1; }
      }
    }
  }
}

// === randomized differential ===

static u64 rng_state = 0x9e3779b97f4a7c15ull;
static u32 rng_u32(u32 bound) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return (u32)(rng_state % bound);
}

static void test_random_differential(void) {
  TEST_BEGIN("randomized differential vs brute-force oracle");
  const u32 N_INSTANCES = 2000;
  for (u32 inst = 0; inst < N_INSTANCES; inst++) {
    CcState *s = cc_init();
    Ref ref; ref_init(&ref);

    // build a parallel pool of nodes in BOTH engines.  We track the
    // (cc node id, ref node id) pair per slot so eq/ne assertions and
    // the final class comparison line up.
    u32 cc_id[REF_MAX_NODES];
    u32 ref_id[REF_MAX_NODES];
    u32 pool = 0;

    u32 n_atoms = 2 + rng_u32(4);            // 2..5 atoms
    for (u32 i = 0; i < n_atoms; i++) {
      cc_id[pool]  = cc_atom(s, i);
      ref_id[pool] = ref_atom(&ref, i);
      pool++;
    }
    u32 n_apps = 2 + rng_u32(8);             // a handful of apps
    for (u32 i = 0; i < n_apps && pool < REF_MAX_NODES - 2; i++) {
      u32 fnl = 1 + rng_u32(3);               // FN_F/FN_G/FN_H
      u32 nargs = 1 + rng_u32(2);            // unary or binary
      u32 cargs[2], rargs[2];
      for (u32 k = 0; k < nargs; k++) {
        u32 src = rng_u32(pool);             // any earlier node
        cargs[k] = cc_id[src];
        rargs[k] = ref_id[src];
      }
      cc_id[pool]  = cc_app(s, fnl, cargs, nargs);
      ref_id[pool] = ref_app(&ref, fnl, rargs, nargs);
      pool++;
    }

    // assert a few random equalities
    u32 n_eq = rng_u32(4);
    for (u32 i = 0; i < n_eq; i++) {
      u32 x = rng_u32(pool), y = rng_u32(pool);
      cc_assert_eq(s, cc_id[x], cc_id[y]);
      ref_union(&ref, ref_id[x], ref_id[y]);
    }
    ref_saturate(&ref);

    // 1. partition agreement: for every pair, cc-equal iff ref-equal.
    for (u32 x = 0; x < pool; x++) {
      for (u32 y = x + 1; y < pool; y++) {
        int cc_eq  = (cc_find(s, cc_id[x]) == cc_find(s, cc_id[y]));
        int ref_eq = (ref_find(&ref, ref_id[x]) == ref_find(&ref, ref_id[y]));
        CHECK(cc_eq == ref_eq);
      }
    }

    // 2. SAT/UNSAT agreement under a random disequality.
    u32 dx = rng_u32(pool), dy = rng_u32(pool);
    cc_assert_ne(s, cc_id[dx], cc_id[dy]);
    int ref_collapsed = (ref_find(&ref, ref_id[dx]) == ref_find(&ref, ref_id[dy]));
    CcResult got = cc_check(s);
    CHECK(got == (ref_collapsed ? CC_UNSAT : CC_SAT));

    cc_free(s);
  }
}

// Nested push/assert/check/pop must match a from-scratch decide at every
// level.  At each level we union a random pair under a fresh push, then
// build a SEPARATE CcState that asserts the SAME accumulated equalities
// from scratch and require identical find-partitions + SAT verdicts.  On
// the way back up, each pop must restore the partition snapshot we took
// before the corresponding push.
static void test_nested_push_pop_differential(void) {
  TEST_BEGIN("nested push/pop matches from-scratch decide at every level");
  const u32 N_TRIALS = 500;
  const u32 DEPTH = 6;
  for (u32 trial = 0; trial < N_TRIALS; trial++) {
    CcState *s = cc_init();
    u32 ids[8];
    u32 n = 4 + rng_u32(4);          // 4..7 atoms
    for (u32 i = 0; i < n; i++) ids[i] = cc_atom(s, i);

    u32 marks[16];
    RtSnap snaps[16];
    // record the accumulated (x,y) eq pairs so the from-scratch oracle
    // can replay them.
    u32 eqx[16], eqy[16];

    for (u32 d = 0; d < DEPTH; d++) {
      rt_snapshot(s, ids, n, &snaps[d]);
      marks[d] = cc_push(s);
      u32 x = rng_u32(n), y = rng_u32(n);
      eqx[d] = x; eqy[d] = y;
      cc_assert_eq(s, ids[x], ids[y]);

      // from-scratch oracle: a fresh state with the same n atoms + all
      // eqs asserted so far (levels 0..d).
      CcState *fresh = cc_init();
      u32 fids[8];
      for (u32 i = 0; i < n; i++) fids[i] = cc_atom(fresh, i);
      for (u32 e = 0; e <= d; e++) cc_assert_eq(fresh, fids[eqx[e]], fids[eqy[e]]);
      RtSnap inc, scr;
      rt_snapshot(s, ids, n, &inc);
      rt_snapshot(fresh, fids, n, &scr);
      CHECK(rt_eq(&inc, &scr));
      cc_free(fresh);
    }
    // unwind: each pop returns to the snapshot taken before its push.
    for (u32 d = DEPTH; d-- > 0;) {
      cc_pop(s, marks[d]);
      RtSnap back; rt_snapshot(s, ids, n, &back);
      CHECK(rt_eq(&back, &snaps[d]));
    }
    cc_free(s);
  }
}

int main(void) {
  test_reflexivity();
  test_transitivity();
  test_congruence_unary();
  test_congruence_binary();
  test_classic_unsat();
  test_classic_sat();
  test_nested();
  test_hashcons();
  test_deep_cascade();
  test_push_pop_roundtrip();
  test_nested_push_pop_differential();
  test_random_differential();
  TEST_REPORT();
}

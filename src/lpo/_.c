// thvm_lpo - Lexicographic Path Ordering on first-order terms.
//
// Compares two terms over a signature of TAG_CTR (function symbols)
// and TAG_FVR (variables) under an LpoConfig (per-symbol precedence
// table).
//
// Returns:
//   LPO_EQ : s and t are structurally identical
//   LPO_GT : s > t under LPO
//   LPO_LT : s < t under LPO
//   LPO_UN : incomparable
//
// Algorithm (Dershowitz, "Orderings for term-rewriting systems",
// 1982).  For terms `s = f(s_1..s_m)` and `t = g(t_1..t_n)`:
//
//   s >_lpo t  iff  one of:
//     (1) s_i >=_lpo t  for some i in 1..m   (subterm domination)
//     (2) f >_F g       AND s >_lpo t_j  for all j in 1..n
//     (3) f == g        AND (s_1..s_m) >_lex_lpo (t_1..t_n)
//                       AND s >_lpo t_j  for all j in 1..n
//
// f >_F g iff precedence[f] > precedence[g]; labels past n_labels read
// precedence 0.  Two distinct symbols of equal precedence are
// incomparable heads (the precedence is a strict partial order), so only
// (1) orders their terms, as Waldmeister's LP_LPO does for
// incomparable ("Unvergleichbar") top symbols (sources/ORD/LPO.c:276-282).
// The lexicographic extension compares the arguments at the first
// position where they differ: when that pair is incomparable, neither
// (s_1..s_m) >_lex (t_1..t_n) nor the converse holds, and s, t are
// ordered by (1) or not at all (LPO.c:300-306; Vampire 5.0.1
// Kernel/LPO.cpp:139-140 cLMA -> cAA, :148-158).
//
// Variable cases:
//   x >_lpo t  iff false (no FVR can dominate any term)
//   s >_lpo x  iff x occurs as a strict subterm of s (and s != x)
//   x >_lpo x  is false (use LPO_EQ on identical FVR ids)
//
// No per-sort awareness in v0; same conservative single-sort
// treatment as KBO.

// Every walk below keeps its pending work on an explicit stack: LPO_WALK_INL
// entries on the C stack, then the heap (term_stack_grow), so term depth
// costs heap, never C stack.  Each visits in the recursion's order.
enum { LPO_WALK_INL = 64 };

// === structural equality (local copy of kbo_eq's pattern) ============

static u8 lpo_eq(Term s, Term t) {
  typedef struct { Term s; Term t; } LpoEqPair;
  LpoEqPair  st_inl[LPO_WALK_INL];
  LpoEqPair *st  = st_inl;
  u32        nst = 0u;
  u32        cst = LPO_WALK_INL;
  u8         eq  = 1u;
  for (;;) {
    if (term_tag(s) != term_tag(t) || term_ext(s) != term_ext(t)) {
      eq = 0u;
    } else {
      switch (term_tag(s)) {
        case TAG_FVR:
          break;   // same id (ext) already checked
        case TAG_CTR: {
          u32 ns = term_ctr_n(s);
          u32 nt = term_ctr_n(t);
          if (ns != nt) {
            eq = 0u;
            break;
          }
          while (nst + ns > cst) {
            st = (LpoEqPair *)term_stack_grow(st, st_inl, &cst, sizeof *st);
          }
          for (u32 i = ns; i-- > 0u;) {
            st[nst].s = term_ctr_at(s, i);
            st[nst].t = term_ctr_at(t, i);
            nst++;
          }
          break;
        }
        default:
          eq = (u8)(term_val(s) == term_val(t));
          break;
      }
    }
    if (!eq || nst == 0u) {
      break;
    }
    nst--;
    s = st[nst].s;
    t = st[nst].t;
  }
  term_stack_release(st, st_inl);
  return eq;
}

// === variable subterm-occurrence check ==============================
// Returns 1 iff FVR `var` (with ext = `var_id`) occurs anywhere in `t`,
// strictly or otherwise.  The strict-subterm caller handles the equality
// case separately.

static u8 lpo_var_occurs_in(Term t, u32 var_id) {
  Term  st_inl[LPO_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = LPO_WALK_INL;
  u8    hit = 0u;
  for (;;) {
    if (term_tag(t) == TAG_FVR) {
      hit = (u8)(term_ext(t) == var_id);
    } else if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (hit || nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return hit;
}

// === pretests (Waldmeister `LPOVortests`) ============================
// Fast pre-checks run before the full recursive descent.  Ported from
// Waldmeister's `LPOVortests` module ("Vortests" = pretests): the
// variable-set comparison ("Variablenmengenvergleich") that cheaply
// rules out incomparable pairs.
//
// Soundness: LPO is a simplification ordering, so `s >lpo t` requires
// every variable occurring in `t` to also occur in `s` (the standard
// LPO variable condition: a variable in t but not s could be
// instantiated to make t arbitrarily large).  Hence if neither
// vars(s) contains vars(t) nor vice versa, the terms are LPO_UN, and
// if one set is a strict superset only that direction can be GT/LT.
// This is a pure optimization: the full recursion reaches the same
// verdict, only slower.

#define LPO_MAX_VAR 64

// OR-accumulate a presence bit per variable id into `seen`.
static void lpo_var_set_acc(Term t, u32 *seen) {
  Term  st_inl[LPO_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = LPO_WALK_INL;
  for (;;) {
    if (term_tag(t) == TAG_FVR) {
      u32 id = term_ext(t);
      if (id < LPO_MAX_VAR) {
        seen[id] = 1;
      }
    } else if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
}

// True iff `var_id < LPO_MAX_VAR` for every FVR reachable in `t`.
// The pretest is only sound when both terms' variable ids are
// bitset-representable; out-of-range ids fall back to the full
// recursion.
static u8 lpo_vars_in_range(Term t) {
  Term  st_inl[LPO_WALK_INL];
  Term *st  = st_inl;
  u32   nst = 0u;
  u32   cst = LPO_WALK_INL;
  u8    in  = 1u;
  for (;;) {
    if (term_tag(t) == TAG_FVR) {
      in = (u8)(term_ext(t) < LPO_MAX_VAR);
    } else if (term_tag(t) == TAG_CTR) {
      u32 n = term_ctr_n(t);
      while (nst + n > cst) {
        st = (Term *)term_stack_grow(st, st_inl, &cst, sizeof *st);
      }
      for (u32 i = n; i-- > 0u;) {
        st[nst++] = term_ctr_at(t, i);
      }
    }
    if (!in || nst == 0u) {
      break;
    }
    t = st[--nst];
  }
  term_stack_release(st, st_inl);
  return in;
}

// Variable-set pretest.  Writes one of LPO_GT / LPO_LT / LPO_EQ /
// LPO_UN into `*out` and returns 1 iff the pretest is conclusive
// enough to skip the full comparison; returns 0 when the full
// recursion is still required.
//
// Returns conclusive 1 only when the verdict is forced:
//   - vars(s) and vars(t) incomparable  -> LPO_UN forced.
// Returns 0 (full recursion needed) for the equal / subset / superset
// cases, because set inclusion alone does not pin down GT vs UN; it
// only rules the opposite direction out, which the full recursion
// then resolves quickly.
static u8 lpo_pretest_varset(Term s, Term t, LpoCmp *out) {
  if (!lpo_vars_in_range(s) || !lpo_vars_in_range(t)) return 0;
  u32 vs[LPO_MAX_VAR] = {0};
  u32 vt[LPO_MAX_VAR] = {0};
  lpo_var_set_acc(s, vs);
  lpo_var_set_acc(t, vt);
  u8 s_has_extra = 0;   // some var in s not in t
  u8 t_has_extra = 0;   // some var in t not in s
  for (u32 i = 0; i < LPO_MAX_VAR; i++) {
    if (vs[i] && !vt[i]) s_has_extra = 1;
    if (vt[i] && !vs[i]) t_has_extra = 1;
  }
  if (s_has_extra && t_has_extra) {
    // Incomparable variable sets: neither s >lpo t nor t >lpo s can
    // hold, and s != t -- so the pair is definitively LPO_UN.
    *out = LPO_UN;
    return 1;
  }
  return 0;
}

// === main comparator =================================================

static LpoCmp lpo_rec(Term s, Term t, const LpoConfig *cfg);

// Memoization of lpo_rec.  The naive LPO recursion is worst-case
// exponential: lpo_some_arg_dominates(s,t), lpo_some_arg_dominates(t,s),
// lpo_dominates_all_args and lpo_lex all recurse on overlapping
// subterm pairs, so the same (s,t) gets recompared up to ~10^5 times
// for the deeply-nested terms that arise on the Sheffer/Nand axioms.
// lpo_rec depends only on (s, t, cfg); cfg is fixed within one
// thvm_lpo call, so caching the verdict per (s,t) pair collapses the
// blow-up to O(|s|*|t|) distinct pairs.  The table is direct-mapped
// and epoch-stamped: thvm_lpo bumps g_lpo_epoch on entry, so stale
// entries from a previous (possibly different-cfg) call are simply not
// matched -- no clearing.  Term cells are pointer-stable within a
// call, so the (s,t) value pair is a sound key.  Hash collisions just
// overwrite, costing a recomputation, never a wrong verdict.
#define LPO_MEMO_BITS 16
#define LPO_MEMO_SIZE (1u << LPO_MEMO_BITS)
#define LPO_MEMO_MASK (LPO_MEMO_SIZE - 1u)
typedef struct { Term s, t; u32 epoch; u8 cmp; } LpoMemoEnt;
static LpoMemoEnt g_lpo_memo[LPO_MEMO_SIZE];
static u32 g_lpo_epoch = 0;
static const LpoConfig *g_lpo_last_cfg = NULL;
// Persistence mode.  OFF (default): thvm_lpo bumps the epoch every call,
// so the memo is fresh per call -- always sound, the right default for
// arbitrary direct callers (tests) that may reuse cell addresses without
// telling us.  ON: the memo persists across calls (huge hit rate on a
// completion) and the caller PROMISES to invalidate on cell movement;
// the ATP engine turns this on at thvm_atp_init and invalidates on every
// GC.  A non-ATP caller never flips it, so it stays sound for them.
static u8 g_lpo_persist = 0;

// === wall-deadline self-abort (Waldmeister has no analog; parity gap) =====
// A single LPO comparison on huge terms is expensive: terms over
// KBO_FLAT_CAP fall to the Term-tree lpo_rec below, whose cost is
// O(|s|*|t|) with a heap_read per child step, so one comparison of two
// ~30k-node terms runs for tens of seconds.  The ATP completion loop only
// polls its wall BETWEEN steps, so such a comparison overruns --wall by
// its full duration (the run must be externally killed, RC=124, instead
// of reporting a clean GaveUp).  The engine installs a zero-arg deadline
// hook here; lpo_rec and the flat and DAG walks poll it every 8192 calls
// and, once it fires, unwind immediately returning LPO_UN.  A verdict is
// perturbed ONLY after the wall has already passed -- by which point the
// run reports GaveUp regardless -- so every comparison that COMPLETES
// before the wall is bit-identical to the un-hooked build.  A non-ATP
// caller never installs a hook, so the poll compiles to a single
// null-pointer test and the default path is unchanged.
static int (*g_lpo_deadline_hook)(void) = NULL;
static int g_lpo_past_deadline = 0;
static u64 g_lpo_poll_ctr      = 0;

fn void thvm_lpo_set_deadline_hook(int (*hook)(void)) {
  g_lpo_deadline_hook = hook;
  g_lpo_past_deadline = 0;
  g_lpo_poll_ctr      = 0;
}

// 1 once the installed wall deadline has passed.  Sticky: the first hook
// hit latches g_lpo_past_deadline so the rest of the in-flight comparison
// unwinds without re-polling.  The clock is sampled at most once per 8192
// calls; between samples this is a predicted-not-taken branch.
static inline int lpo_deadline_fired(void) {
  if (g_lpo_deadline_hook == NULL) return 0;
  if (g_lpo_past_deadline) return 1;
  if ((++g_lpo_poll_ctr & 8191u) != 0u) return 0;
  if (g_lpo_deadline_hook()) { g_lpo_past_deadline = 1; return 1; }
  return 0;
}

static inline u32 lpo_memo_hash(Term s, Term t) {
  u64 h = (s * 0x9E3779B97F4A7C15ull) ^ (t * 0xBF58476D1CE4E5B9ull);
  h ^= h >> 29;
  return (u32)h & LPO_MEMO_MASK;
}

// lpo_rec on an explicit stack.  One frame per open lpo_rec_compute(s, t)
// of the recursion this replaces: `step` is the case it is evaluating and
// `i` the argument it has reached; its verdict is memoized at slot `idx`
// when the frame finishes.  A call lpo_rec(x, y) settles at once
// (identical cells, a fired deadline, a memo hit) or opens a frame, and
// its verdict goes back to the frame that made it.
//
// For s = f(s_1..s_m), t = g(t_1..t_n), after equality and the variable
// cases: case (1) both ways (some s_i >= t gives GT, some t_j >= s gives
// LT); then the top symbols -- f >_F g needs s > every t_j (GT, else
// UN), g >_F f the converse, and equal heads compare the arguments
// lexicographically, the winner needing to dominate the loser's
// arguments.
typedef struct {
  Term s;
  Term t;
  u32  idx;
  u32  i;
  u8   step;
} LpoRecFrame;
enum {
  LPO_R_BEGIN,     // equality, variables, then case (1)
  LPO_R_SOME_ST,   // case (1): some s_i >=_lpo t
  LPO_R_SOME_TS,   //           some t_j >=_lpo s
  LPO_R_DOM_ST,    // s >_lpo every t_j: GT, else UN
  LPO_R_DOM_TS,    // t >_lpo every s_i: LT, else UN
  LPO_R_LEX        // case (3): the first argument pair that is not EQ
};

static LpoCmp lpo_rec(Term s, Term t, const LpoConfig *cfg) {
  if (s == t) return LPO_EQ;            // pointer-identical, no memo
  if (lpo_deadline_fired()) return LPO_UN;   // wall passed: unwind fast
  u32 idx = lpo_memo_hash(s, t);
  LpoMemoEnt *e = &g_lpo_memo[idx];
  if (e->epoch == g_lpo_epoch && e->s == s && e->t == t)
    return (LpoCmp)(i8)e->cmp;
  LpoRecFrame  fr_inl[LPO_WALK_INL];
  LpoRecFrame *fr  = fr_inl;
  u32          nfr = 1u;
  u32          cfr = LPO_WALK_INL;
  fr[0].s    = s;
  fr[0].t    = t;
  fr[0].idx  = idx;
  fr[0].i    = 0u;
  fr[0].step = LPO_R_BEGIN;
  LpoCmp val  = LPO_UN;   // a call's verdict, pending for the top frame
  u8     have = 0u;
  for (;;) {
    LpoRecFrame *f   = &fr[nfr - 1u];
    u8           fin = 0u;
    LpoCmp       res = LPO_UN;
    if (have) {
      have = 0u;
      switch (f->step) {
        case LPO_R_SOME_ST:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_GT;
          } else {
            f->i++;
          }
          break;
        case LPO_R_SOME_TS:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_LT;
          } else {
            f->i++;
          }
          break;
        case LPO_R_DOM_ST:
        case LPO_R_DOM_TS:
          if (val != LPO_GT) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->i++;
          }
          break;
        default:   // LPO_R_LEX
          if (val == LPO_EQ) {
            f->i++;
          } else if (val == LPO_GT) {
            f->step = LPO_R_DOM_ST;
            f->i    = 0u;
          } else if (val == LPO_LT) {
            f->step = LPO_R_DOM_TS;
            f->i    = 0u;
          } else {
            // An incomparable argument pair decides neither lex direction.
            fin = 1u;
            res = LPO_UN;
          }
          break;
      }
    }
    Term x    = 0;
    Term y    = 0;
    u8   call = 0u;
    while (!fin && !call) {
      switch (f->step) {
        case LPO_R_BEGIN: {
          if (lpo_eq(f->s, f->t)) {
            fin = 1u;
            res = LPO_EQ;
            break;
          }
          u8 s_is_fvr = (term_tag(f->s) == TAG_FVR);
          u8 t_is_fvr = (term_tag(f->t) == TAG_FVR);
          if (s_is_fvr && t_is_fvr) {
            // Distinct FVR ids: incomparable.
            fin = 1u;
            res = LPO_UN;
          } else if (s_is_fvr) {
            // s >_lpo t is false; t > x iff x occurs in t.
            fin = 1u;
            res = lpo_var_occurs_in(f->t, term_ext(f->s)) ? LPO_LT : LPO_UN;
          } else if (t_is_fvr) {
            // s >_lpo x iff x occurs strictly in s.
            fin = 1u;
            res = lpo_var_occurs_in(f->s, term_ext(f->t)) ? LPO_GT : LPO_UN;
          } else if (term_tag(f->s) != TAG_CTR || term_tag(f->t) != TAG_CTR) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->step = LPO_R_SOME_ST;
            f->i    = 0u;
          }
          break;
        }
        case LPO_R_SOME_ST:
          if (f->i < term_ctr_n(f->s)) {
            x    = term_ctr_at(f->s, f->i);
            y    = f->t;
            call = 1u;
          } else {
            f->step = LPO_R_SOME_TS;
            f->i    = 0u;
          }
          break;
        case LPO_R_SOME_TS:
          if (f->i < term_ctr_n(f->t)) {
            x    = term_ctr_at(f->t, f->i);
            y    = f->s;
            call = 1u;
          } else {
            u32 lab_s  = term_ext(f->s);
            u32 lab_t  = term_ext(f->t);
            u32 prec_s = (lab_s < cfg->n_labels) ? cfg->precedence[lab_s] : 0;
            u32 prec_t = (lab_t < cfg->n_labels) ? cfg->precedence[lab_t] : 0;
            if (prec_s > prec_t) {
              f->step = LPO_R_DOM_ST;
              f->i    = 0u;
            } else if (prec_s < prec_t) {
              f->step = LPO_R_DOM_TS;
              f->i    = 0u;
            } else if (lab_s != lab_t || term_ctr_n(f->s) != term_ctr_n(f->t)) {
              fin = 1u;
              res = LPO_UN;
            } else {
              f->step = LPO_R_LEX;
              f->i    = 0u;
            }
          }
          break;
        case LPO_R_DOM_ST:
          if (f->i < term_ctr_n(f->t)) {
            x    = f->s;
            y    = term_ctr_at(f->t, f->i);
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_GT;
          }
          break;
        case LPO_R_DOM_TS:
          if (f->i < term_ctr_n(f->s)) {
            x    = f->t;
            y    = term_ctr_at(f->s, f->i);
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_LT;
          }
          break;
        default:   // LPO_R_LEX
          if (f->i < term_ctr_n(f->s)) {
            x    = term_ctr_at(f->s, f->i);
            y    = term_ctr_at(f->t, f->i);
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_EQ;   // can't happen: lpo_eq fired earlier
          }
          break;
      }
    }
    if (call) {
      if (x == y) {
        val  = LPO_EQ;
        have = 1u;
        continue;
      }
      if (lpo_deadline_fired()) {
        val  = LPO_UN;
        have = 1u;
        continue;
      }
      u32 cidx = lpo_memo_hash(x, y);
      LpoMemoEnt *ce = &g_lpo_memo[cidx];
      if (ce->epoch == g_lpo_epoch && ce->s == x && ce->t == y) {
        val  = (LpoCmp)(i8)ce->cmp;
        have = 1u;
        continue;
      }
      if (nfr == cfr) {
        fr = (LpoRecFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].s    = x;
      fr[nfr].t    = y;
      fr[nfr].idx  = cidx;
      fr[nfr].i    = 0u;
      fr[nfr].step = LPO_R_BEGIN;
      nfr++;
      continue;
    }
    LpoMemoEnt *me = &g_lpo_memo[f->idx];
    CELL_MEMO_WRITE();
    me->s = f->s; me->t = f->t; me->epoch = g_lpo_epoch; me->cmp = (u8)res;
    if (--nfr == 0u) {
      term_stack_release(fr, fr_inl);
      return res;
    }
    val  = res;
    have = 1u;
  }
}

// Invalidate the whole (s,t)->verdict memo in O(1) by bumping the epoch.
// The verdict depends only on (s, t, precedence); both s,t cells (until
// the GC moves them) and the precedence (within one run) are stable, so
// the memo may PERSIST across thvm_lpo calls -- which is the whole point:
// on a completion the same subterm pairs are compared millions of times.
// Callers must invalidate when those invariants break: the ATP engine
// calls this on every GC (cells move) and at thvm_atp_init (a new run may
// reuse a static LpoConfig pointer with different precedence content).
fn void thvm_lpo_invalidate(void) {
  if (++g_lpo_epoch == 0) {
    for (u32 i = 0; i < LPO_MEMO_SIZE; i++) g_lpo_memo[i].epoch = 0;
    g_lpo_epoch = 1;
  }
}

// The ATP engine opts in to persistent memoization (and promises to
// invalidate on GC).  Default off keeps thvm_lpo sound for any caller.
fn void thvm_lpo_set_persist(u8 on) { g_lpo_persist = on ? 1u : 0u; }

// === flatterm-spine subterm pretest (Waldmeister LV_VortestLPOGroesser) ====
// Single linear pass over the KboFlatNode encoding of a and b.  Detects the
// two cheap conclusive verdicts that dominate Waldmeister's hot loop:
//
//   (a) b is a strict preorder slice (= subterm) of a              -> LPO_GT
//   (b) a is a strict preorder slice (= subterm) of b              -> LPO_LT
//   (c) one operand is a constant c: a term t > c iff a symbol of t
//       is above c or c occurs in t; c > t iff t is ground and
//       every symbol of t is below c                               -> +/- forced
//
// Returns +1 / -1 if conclusive (caller maps to LPO_GT / LPO_LT), or 0
// to fall through to the full recursive thvm_lpo (then the memoized
// lpo_rec resolves the residual hard cases).  See
// waldmeister/sources/ORD/LPOVortests.c:436 LV_VortestLPOGroesser for the
// reference algorithm (Termliste candidate scan).

#define LPO_FLAT_KAND_CAP 256

static inline __attribute__((always_inline))
int lpo_pretest_groesser_flat(const KboFlatNode *a, u32 na,
                              const KboFlatNode *b, u32 nb,
                              const LpoConfig *cfg) {
  if (na == 0 || nb == 0) return 0;
  // VortestLPOGroesserVar.
  if (a[0].sym < 0) return -1;            // var never strictly dominates
  if (b[0].sym < 0) {
    i32 vs = b[0].sym;
    // b is a variable -- a > b iff that var occurs strictly inside a.
    for (u32 i = 1; i < na; i++) if (a[i].sym == vs) return +1;
    return -1;
  }
  // VortestLPOGroesserKonst -- a is a constant (no children, sz == 1).
  if (a[0].sz == 1u) {
    u32 pa = ((u32)a[0].sym < cfg->n_labels) ? cfg->precedence[(u32)a[0].sym] : 0u;
    for (u32 i = 0; i < nb; i++) {
      if (b[i].sym < 0) return -1;        // var in b -- const cannot dominate
      u32 pb_i = ((u32)b[i].sym < cfg->n_labels) ? cfg->precedence[(u32)b[i].sym] : 0u;
      if (pb_i >= pa) return -1;          // some symbol in b matches or beats a
    }
    return +1;
  }
  if (b[0].sz == 1u) {
    u32 pb = ((u32)b[0].sym < cfg->n_labels) ? cfg->precedence[(u32)b[0].sym] : 0u;
    for (u32 i = 0; i < na; i++) {
      if (a[i].sym < 0) continue;
      u32 pa_i = ((u32)a[i].sym < cfg->n_labels) ? cfg->precedence[(u32)a[i].sym] : 0u;
      if (pa_i > pb || a[i].sym == b[0].sym) return +1;
    }
    return -1;
  }

  // Fast-fail: b can only be a strict subterm of a if a has strictly more
  // nodes.  na <= nb rules out the subterm direction so the general scan
  // below would always return 0 (inconclusive).  Return 0 immediately
  // without paying the candidate-list cost.
  if (na <= nb) return 0;
  // Direct subterm search: walk a's preorder positions; at each i where
  // a[i].sym == b[0].sym AND a[i].sz == b[0].sz, check the slice
  // a[i..i+nb) against b[0..nb).  Simpler than the candidate-list
  // approach -- no per-outer-step bookkeeping -- and faster when
  // matches at b[0].sym are common (each candidate would otherwise pile
  // up in the parallel scan).  i=0 is excluded for strict-subterm.
  for (u32 i = 1; i + nb <= na; i++) {
    if (a[i].sym != b[0].sym) continue;
    if (a[i].sz  != b[0].sz)  continue;
    u8 match = 1;
    for (u32 j = 1; j < nb; j++) {
      if (a[i + j].sym != b[j].sym) { match = 0; break; }
    }
    if (match) return +1;
  }
  return 0;
}

// Cached encode buffers for the pretest.  Reused across calls in the same
// thvm_lpo invocation; thvm_kbo_flat reuses g_kbo_flat_a/b in its own way
// so we maintain a private pair to avoid stomping on a concurrent KBO call.
static KboFlatNode g_lpo_flat_a[KBO_FLAT_CAP];
static KboFlatNode g_lpo_flat_b[KBO_FLAT_CAP];

// === flatterm recursive LPO body (Waldmeister LPO.c port) =================
// Operates on KboFlatNode slices instead of Term trees: children iteration
// is array-indexed (next child = cur + node[cur].sz) instead of going
// through term_ctr_at which descends via heap reads.  The memo is keyed
// on (pa, pb) -- positions within the per-call encode buffers -- which is
// a much smaller key space than (Term, Term) and stays stable for the
// duration of one thvm_lpo call.

// Position-keyed (pa, pb, side) memo for the flat walk.  Raised 14 -> 20
// (16K -> 1M entries, ~16 MB) together with KBO_FLAT_CAP 4096 -> 65536:
// once ~32k-node peaks stay on the flat path, one comparison touches a
// position space of up to |s|*|t| ~ 10^9, so a 16K direct-mapped memo
// thrashed (evicting entries the recursion re-reaches) and collapsed the
// verdict back toward the un-memoized exponential.  1M entries keep the
// reached-pair working set resident, holding the comparison at its
// O(|s|*|t|) polynomial bound.  Pure speed: a collision recomputes, never
// returns a stale verdict (epoch + (pa,pb,side) are re-checked on hit).
#define LPO_FLAT_MEMO_BITS 20
#define LPO_FLAT_MEMO_SIZE (1u << LPO_FLAT_MEMO_BITS)
#define LPO_FLAT_MEMO_MASK (LPO_FLAT_MEMO_SIZE - 1u)

// Diagnostic counters for the flat and DAG walks.  Exposed via
// thvm_lpo_flat_stats.
static u64 g_lpo_flat_rec_calls    = 0;   // total wrapper invocations
static u64 g_lpo_flat_memo_hits    = 0;   // (pa,pb,side,epoch) cache hit
static u64 g_lpo_flat_compute_calls = 0;  // compute body invocations
static u64 g_lpo_top_calls         = 0;   // top-level thvm_lpo calls
fn void thvm_lpo_flat_stats(u64 *rec_calls, u64 *memo_hits,
                            u64 *compute_calls, u64 *top_calls) {
  if (rec_calls)     *rec_calls     = g_lpo_flat_rec_calls;
  if (memo_hits)     *memo_hits     = g_lpo_flat_memo_hits;
  if (compute_calls) *compute_calls = g_lpo_flat_compute_calls;
  if (top_calls)     *top_calls     = g_lpo_top_calls;
}
fn void thvm_lpo_flat_stats_reset(void) {
  g_lpo_flat_rec_calls = g_lpo_flat_memo_hits = 0;
  g_lpo_flat_compute_calls = g_lpo_top_calls = 0;
}
// `side` encodes which buffer holds the lhs: 0 = g_lpo_flat_a, 1 = g_lpo_flat_b.
// (pa, pb, side) -- without `side`, swapped recursive calls (a-vs-b and b-vs-a)
// collide on (pa, pb) and pollute each other's verdicts.
typedef struct { u32 pa, pb; u32 epoch; u8 cmp; u8 side; } LpoFlatMemoEnt;
static LpoFlatMemoEnt g_lpo_flat_memo[LPO_FLAT_MEMO_SIZE];
static u32 g_lpo_flat_epoch = 0;

// LPO-specific flatterm encoder.  Bypasses term_ctr_at's redundant
// term_ctr_n call per child access (which costs an extra heap_read per
// iteration).  Stores precedence directly in .w via the LpoConfig
// precedence table -- the inner recursion reads a[pa].w instead of
// indexing cfg->precedence[sym] per node.
static u8 lpo_flat_encode_rec(Term t, const LpoConfig *cfg,
                              KboFlatNode *out, u32 *pos) {
  // One frame per open CTR: its slot `here` (its span is set when its
  // last child is done) and the child `i` being encoded.
  typedef struct { u64 base; u32 here; u32 i; u32 n; } LpoEncFrame;
  LpoEncFrame  fr_inl[LPO_WALK_INL];
  LpoEncFrame *fr  = fr_inl;
  u32          nfr = 0u;
  u32          cfr = LPO_WALK_INL;
  u8           ok  = 1u;
  for (;;) {
    u32 here = *pos;
    if (__builtin_expect(here >= KBO_FLAT_CAP, 0)) {
      ok = 0u;
      break;
    }
    *pos = here + 1u;
    u32 tag = term_tag(t);
    if (tag == TAG_FVR) {
      out[here].sym = -(i32)(term_ext(t) + 1u);
      out[here].w   = 0;
      out[here].sz  = 1u;
    } else if (__builtin_expect(tag != TAG_CTR, 0)) {
      ok = 0u;
      break;
    } else {
      u32 lab = term_ext(t);
      out[here].sym = (i32)lab;
      out[here].w   = (lab < cfg->n_labels) ? (i32)cfg->precedence[lab] : 0;
      // Cache the children base loc once -- term_ctr_at would re-read
      // the arity cell via term_ctr_n per call.
      u64 base = term_val(t);
      Term n_cell = heap_read(base);
      if (__builtin_expect(term_tag(n_cell) != TAG_NUM, 0)) {
        out[here].sz = 1u;
      } else {
        u32 n = (u32)term_val(n_cell);
        if (n > 0u) {
          if (nfr == cfr) {
            fr = (LpoEncFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].base = base;
          fr[nfr].here = here;
          fr[nfr].i    = 0u;
          fr[nfr].n    = n;
          nfr++;
          t = heap_read(base + 1u);
          continue;
        }
        out[here].sz = *pos - here;
      }
    }
    u8 next = 0u;
    while (nfr > 0u) {
      LpoEncFrame *f = &fr[nfr - 1u];
      f->i++;
      if (f->i < f->n) {
        t    = heap_read(f->base + 1u + (u64)f->i);
        next = 1u;
        break;
      }
      out[f->here].sz = *pos - f->here;
      nfr--;
    }
    if (!next) {
      break;
    }
  }
  term_stack_release(fr, fr_inl);
  return ok;
}

static inline u32 lpo_flat_hash(u32 pa, u32 pb, u8 side) {
  u64 h = ((u64)pa * 0x9E3779B97F4A7C15ull) ^ ((u64)pb * 0xBF58476D1CE4E5B9ull);
  h ^= ((u64)side * 0x94D049BB133111EBull);
  h ^= h >> 29;
  return (u32)h & LPO_FLAT_MEMO_MASK;
}

static inline __attribute__((always_inline))
u8 lpo_flat_slice_eq(const KboFlatNode *a, u32 pa,
                     const KboFlatNode *b, u32 pb) {
  if (a[pa].sym != b[pb].sym) return 0;
  if (a[pa].sz != b[pb].sz)   return 0;
  u32 n = a[pa].sz;
  for (u32 i = 1; i < n; i++) {
    if (a[pa + i].sym != b[pb + i].sym) return 0;
  }
  return 1;
}

static u8 lpo_flat_var_occurs(const KboFlatNode *t, u32 pos, i32 vsym) {
  u32 end = pos + t[pos].sz;
  for (u32 i = pos; i < end; i++) if (t[i].sym == vsym) return 1;
  return 0;
}

// lpo_flat_rec_compute on an explicit stack, as lpo_rec above: one frame
// per open compute of the recursion this replaces.  (a, pa) and (b, pb)
// are the two subterms (either array may be on either side), `cur` /
// `cur2` the child cursors the step has reached; a wrapped frame (one
// lpo_flat_rec opened) memoizes its verdict at `idx` / `side`, the root
// (lpo_flat_ref) is unwrapped.
//
// After slice equality, the variable cases and the per-level Vortest:
// prec(a) > prec(b) needs a > every child of b (GT), else some child of
// b >= a gives LT; the converse for prec(a) < prec(b); equal precedences
// try case (1) both ways, then (same symbol) the children
// lexicographically, the winner dominating the loser's children from
// the differing one on (WM's LPOGroesserAlleDiff, LPO.c:261/287).
typedef struct {
  const KboFlatNode *a;
  const KboFlatNode *b;
  u32 pa;
  u32 pb;
  u32 cur;
  u32 cur2;
  u32 idx;
  u8  side;
  u8  wrapped;
  u8  step;
} LpoFlatFrame;
enum {
  LPO_F_BEGIN,
  LPO_F_GT_DOM,     // prec(a) > prec(b): a > every child of b -> GT
  LPO_F_GT_SOME,    //   else some child of b >= a -> LT, else UN
  LPO_F_LT_DOM,     // prec(a) < prec(b): b > every child of a -> LT
  LPO_F_LT_SOME,    //   else some child of a >= b -> GT, else UN
  LPO_F_EQ_SOME_A,  // equal: some child of a >= b -> GT
  LPO_F_EQ_SOME_B,  //        some child of b >= a -> LT
  LPO_F_LEX,        //        children pairwise, the first not EQ
  LPO_F_FROM_B,     // lex GT: a > every child of b from cur -> GT, else UN
  LPO_F_FROM_A      // lex LT: b > every child of a from cur -> LT, else UN
};

static LpoCmp lpo_flat_run(const KboFlatNode *a, u32 pa,
                           const KboFlatNode *b, u32 pb,
                           const LpoConfig *cfg) {
  LpoFlatFrame  fr_inl[LPO_WALK_INL];
  LpoFlatFrame *fr  = fr_inl;
  u32           nfr = 1u;
  u32           cfr = LPO_WALK_INL;
  fr[0].a       = a;
  fr[0].b       = b;
  fr[0].pa      = pa;
  fr[0].pb      = pb;
  fr[0].cur     = 0u;
  fr[0].cur2    = 0u;
  fr[0].idx     = 0u;
  fr[0].side    = 0u;
  fr[0].wrapped = 0u;
  fr[0].step    = LPO_F_BEGIN;
  LpoCmp val  = LPO_UN;   // a call's verdict, pending for the top frame
  u8     have = 0u;
  for (;;) {
    LpoFlatFrame      *f   = &fr[nfr - 1u];
    const KboFlatNode *fa  = f->a;
    const KboFlatNode *fb  = f->b;
    u8                 fin = 0u;
    LpoCmp             res = LPO_UN;
    if (have) {
      have = 0u;
      switch (f->step) {
        case LPO_F_GT_DOM:
          if (val != LPO_GT) {
            f->step = LPO_F_GT_SOME;
            f->cur  = f->pb + 1u;
          } else {
            f->cur += fb[f->cur].sz;
          }
          break;
        case LPO_F_LT_DOM:
          if (val != LPO_GT) {
            f->step = LPO_F_LT_SOME;
            f->cur  = f->pa + 1u;
          } else {
            f->cur += fa[f->cur].sz;
          }
          break;
        case LPO_F_GT_SOME:
        case LPO_F_EQ_SOME_B:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_LT;
          } else {
            f->cur += fb[f->cur].sz;
          }
          break;
        case LPO_F_LT_SOME:
        case LPO_F_EQ_SOME_A:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_GT;
          } else {
            f->cur += fa[f->cur].sz;
          }
          break;
        case LPO_F_LEX:
          if (val == LPO_EQ) {
            f->cur  += fa[f->cur].sz;
            f->cur2 += fb[f->cur2].sz;
          } else if (val == LPO_GT) {
            f->step = LPO_F_FROM_B;
            f->cur  = f->cur2;
          } else if (val == LPO_LT) {
            f->step = LPO_F_FROM_A;
          } else {
            // An incomparable argument pair decides neither lex direction,
            // and case (1) failed both ways above (LPO.c:300-306).
            fin = 1u;
            res = LPO_UN;
          }
          break;
        case LPO_F_FROM_B:
          if (val != LPO_GT) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->cur += fb[f->cur].sz;
          }
          break;
        default:   // LPO_F_FROM_A
          if (val != LPO_GT) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->cur += fa[f->cur].sz;
          }
          break;
      }
    }
    // The next call lpo_flat_rec(x, px, y, py), or the verdict.
    const KboFlatNode *x  = NULL;
    const KboFlatNode *y  = NULL;
    u32                px = 0u;
    u32                py = 0u;
    u8                 call = 0u;
    while (!fin && !call) {
      u32 ea = f->pa + fa[f->pa].sz;
      u32 eb = f->pb + fb[f->pb].sz;
      switch (f->step) {
        case LPO_F_BEGIN: {
          if (lpo_flat_slice_eq(fa, f->pa, fb, f->pb)) {
            fin = 1u;
            res = LPO_EQ;
            break;
          }
          u8 a_is_var = (fa[f->pa].sym < 0);
          u8 b_is_var = (fb[f->pb].sym < 0);
          if (__builtin_expect(a_is_var | b_is_var, 0)) {
            fin = 1u;
            if (a_is_var && b_is_var) {
              res = LPO_UN;
            } else if (a_is_var) {
              res = lpo_flat_var_occurs(fb, f->pb, fa[f->pa].sym) ? LPO_LT : LPO_UN;
            } else {
              res = lpo_flat_var_occurs(fa, f->pa, fb[f->pb].sym) ? LPO_GT : LPO_UN;
            }
            break;
          }
          // Per-level subterm Vortest: pretest at each recursion level
          // catches the strict-subterm cases without paying the cost of the
          // some_arg_dom recursion below.  Mirrors WM's LP_LPOGroesser
          // structure where each recursive call enters via
          // LV_VortestLPOGroesser before the some-arg-dominates dispatch.
          if (lpo_pretest_groesser_flat(fa + f->pa, fa[f->pa].sz,
                                        fb + f->pb, fb[f->pb].sz, cfg) > 0) {
            fin = 1u;
            res = LPO_GT;
            break;
          }
          if (lpo_pretest_groesser_flat(fb + f->pb, fb[f->pb].sz,
                                        fa + f->pa, fa[f->pa].sz, cfg) > 0) {
            fin = 1u;
            res = LPO_LT;
            break;
          }
          // Precedence dispatch first (WM's structure): when precs differ,
          // case (2) dominates_all_args resolves directly; case (1) only
          // fires for the OTHER direction.  When precs are equal, lex +
          // case (3) applies and case (1) is checked symmetrically.
          i32 prec_a = fa[f->pa].w;
          i32 prec_b = fb[f->pb].w;
          if (__builtin_expect(prec_a > prec_b, 0)) {
            f->step = LPO_F_GT_DOM;
            f->cur  = f->pb + 1u;
          } else if (__builtin_expect(prec_a < prec_b, 0)) {
            f->step = LPO_F_LT_DOM;
            f->cur  = f->pa + 1u;
          } else {
            f->step = LPO_F_EQ_SOME_A;
            f->cur  = f->pa + 1u;
          }
          break;
        }
        case LPO_F_GT_DOM:
          if (f->cur < eb) {
            x = fa; px = f->pa; y = fb; py = f->cur;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_GT;
          }
          break;
        case LPO_F_GT_SOME:
          if (f->cur < eb) {
            x = fb; px = f->cur; y = fa; py = f->pa;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_UN;
          }
          break;
        case LPO_F_LT_DOM:
          if (f->cur < ea) {
            x = fb; px = f->pb; y = fa; py = f->cur;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_LT;
          }
          break;
        case LPO_F_LT_SOME:
          if (f->cur < ea) {
            x = fa; px = f->cur; y = fb; py = f->pb;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_UN;
          }
          break;
        case LPO_F_EQ_SOME_A:
          if (f->cur < ea) {
            x = fa; px = f->cur; y = fb; py = f->pb;
            call = 1u;
          } else {
            f->step = LPO_F_EQ_SOME_B;
            f->cur  = f->pb + 1u;
          }
          break;
        case LPO_F_EQ_SOME_B:
          if (f->cur < eb) {
            x = fb; px = f->cur; y = fa; py = f->pa;
            call = 1u;
          } else if (fa[f->pa].sym != fb[f->pb].sym) {
            fin = 1u;
            res = LPO_UN;
          } else {
            // Same head symbol implies same arity (signature constraint),
            // but subtree sizes (`sz`) may differ when children have
            // different shapes -- so DO NOT short-circuit on sz mismatch.
            f->step = LPO_F_LEX;
            f->cur  = f->pa + 1u;
            f->cur2 = f->pb + 1u;
          }
          break;
        case LPO_F_LEX:
          if (f->cur < ea && f->cur2 < eb) {
            x = fa; px = f->cur; y = fb; py = f->cur2;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_EQ;
          }
          break;
        case LPO_F_FROM_B:
          if (f->cur < eb) {
            x = fa; px = f->pa; y = fb; py = f->cur;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_GT;
          }
          break;
        default:   // LPO_F_FROM_A
          if (f->cur < ea) {
            x = fb; px = f->pb; y = fa; py = f->cur;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_LT;
          }
          break;
      }
    }
    if (call) {
      // lpo_flat_rec(x, px, y, py): the memoized wrapper.
      g_lpo_flat_rec_calls++;
      if (x == y && px == py) {
        val  = LPO_EQ;
        have = 1u;
        continue;
      }
      if (lpo_deadline_fired()) {   // wall passed: unwind fast
        val  = LPO_UN;
        have = 1u;
        continue;
      }
      u8  side = (x == g_lpo_flat_a) ? 0u : 1u;
      u32 cidx = lpo_flat_hash(px, py, side);
      LpoFlatMemoEnt *ce = &g_lpo_flat_memo[cidx];
      if (ce->epoch == g_lpo_flat_epoch && ce->pa == px && ce->pb == py &&
          ce->side == side) {
        g_lpo_flat_memo_hits++;
        val  = (LpoCmp)(i8)ce->cmp;
        have = 1u;
        continue;
      }
      g_lpo_flat_compute_calls++;
      if (nfr == cfr) {
        fr = (LpoFlatFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      LpoFlatFrame *nf = &fr[nfr++];
      nf->a       = x;
      nf->b       = y;
      nf->pa      = px;
      nf->pb      = py;
      nf->cur     = 0u;
      nf->cur2    = 0u;
      nf->idx     = cidx;
      nf->side    = side;
      nf->wrapped = 1u;
      nf->step    = LPO_F_BEGIN;
      continue;
    }
    if (f->wrapped) {
      LpoFlatMemoEnt *me = &g_lpo_flat_memo[f->idx];
      me->pa = f->pa; me->pb = f->pb; me->epoch = g_lpo_flat_epoch;
      me->cmp = (u8)res; me->side = f->side;
    }
    if (--nfr == 0u) {
      term_stack_release(fr, fr_inl);
      return res;
    }
    val  = res;
    have = 1u;
  }
}

// The flat recursion on g_lpo_flat_a / g_lpo_flat_b as encoded, with a
// fresh position memo: the reference verdict lpo_flat_cmp reproduces.
static LpoCmp lpo_flat_ref(const LpoConfig *cfg) {
  if (++g_lpo_flat_epoch == 0) {
    for (u32 i = 0; i < LPO_FLAT_MEMO_SIZE; i++) {
      g_lpo_flat_memo[i].epoch = 0;
    }
    g_lpo_flat_epoch = 1;
  }
  return lpo_flat_run(g_lpo_flat_a, 0u, g_lpo_flat_b, 0u, cfg);
}

// === DAG comparator (the flat path's evaluator) ==========================
// A flat-walk compute (lpo_flat_run) is a pure function of the two
// subterms it is handed: slice equality, variable occurrence, the Vortest pretests, the
// precedences and the children it recurses on all read only the two
// slices.  So its verdict can be memoized on subterm CONTENT instead of
// position.  lpo_flat_cmp hash-conses both flat arrays into one DAG
// (equal subterms share a class) and lpo_dag_compute runs the same case
// analysis, in the same order, over class pairs with a lossless memo:
// each class pair is computed once, and every per-pair test is O(1)
// from per-class summaries except the strict-subterm Vortest, a binary
// search in an occurrence list.  The position memo above is direct-
// mapped, so once a compare reaches more pairs than it holds (the deep,
// repetitive terms of a run-order interreduction, ten thousand nodes and
// more) evicted pairs are recomputed, each with O(|slice|) Vortest
// scans, and one compare ran for tens of millions of steps.
//
// Class equality is slice equality only when every symbol has one arity
// and one precedence across both operands (the preorder symbol sequence
// then parses to one tree).  A compare that breaks this goes to the flat
// recursion unchanged (lpo_flat_ref).

typedef struct {
  i32 sym;      // KboFlatNode.sym of the class root
  i32 w;        // KboFlatNode.w: the precedence the recursion dispatches on
  u32 p;        // the precedence the Vortest reads (0 past n_labels)
  u32 sz;       // nodes in the subterm
  u32 arity;
  u32 kids;     // first child class in g_ld_kids
  u32 maxp;     // max p over the function-symbol nodes of the subterm
  u32 rep;      // one occurrence: (side << 31) | position
  u64 vmask;    // variables with id < 64 in the subterm
  u8  hasvar;
  u8  bigvar;   // a variable with id >= 64 in the subterm
} LpoDagNode;

typedef struct {
  i32 sym;
  i32 w;
  u32 arity;
  u32 used;
} LpoDagSym;

typedef struct {
  u32 k1;       // c1 + 1; 0 marks an empty slot
  u32 k2;
  u32 v;
} LpoDagMemo;

// Memo slots one compare may grow to (12 bytes each), and the size above
// which the next compare frees the table instead of reusing it.
#define LPO_DAG_MEMO_MAX  (1u << 25)
#define LPO_DAG_MEMO_KEEP (1u << 20)

static LpoDagNode        *g_ld_node      = NULL;
static u32               *g_ld_kids      = NULL;
static u32               *g_ld_cls[2]    = {NULL, NULL};
static u32               *g_ld_occ_beg[2] = {NULL, NULL};
static u32               *g_ld_occ_pos[2] = {NULL, NULL};
static u32               *g_ld_tab       = NULL;
static LpoDagSym         *g_ld_sym       = NULL;
static u32                g_ld_cap       = 0;
static u32                g_ld_tab_mask  = 0;
static LpoDagMemo        *g_ld_memo      = NULL;
static u32                g_ld_memo_cap  = 0;
static u32                g_ld_memo_mask = 0;
static u32                g_ld_memo_cnt  = 0;
static u8                 g_ld_memo_full = 0;
static u32                g_ld_n         = 0;
static u32                g_ld_nkids     = 0;
static const KboFlatNode *g_ld_arr[2]    = {NULL, NULL};
static u32                g_ld_len[2]    = {0, 0};
static u8                 g_ld_occ_ok[2] = {0, 0};
static u64                g_lpo_dag_fallbacks = 0;

static void lpo_dag_free(void) {
  free(g_ld_node);
  free(g_ld_kids);
  free(g_ld_tab);
  free(g_ld_sym);
  for (u32 s = 0; s < 2u; s++) {
    free(g_ld_cls[s]);
    free(g_ld_occ_beg[s]);
    free(g_ld_occ_pos[s]);
    g_ld_cls[s] = g_ld_occ_beg[s] = g_ld_occ_pos[s] = NULL;
  }
  g_ld_node = NULL;
  g_ld_kids = g_ld_tab = NULL;
  g_ld_sym  = NULL;
  g_ld_cap  = 0;
}

// Size the per-compare arrays for n = na + nb nodes and clear the two
// hash tables over the slots this compare uses.  0 on allocation
// failure (the caller then takes the flat recursion).
static u8 lpo_dag_reserve(u32 n) {
  if (n > g_ld_cap) {
    u32 cap = 256u;
    while (cap < n) {
      cap <<= 1;
    }
    lpo_dag_free();
    g_ld_node = (LpoDagNode *)malloc((size_t)cap * sizeof(LpoDagNode));
    g_ld_kids = (u32 *)malloc((size_t)cap * sizeof(u32));
    g_ld_tab  = (u32 *)malloc((size_t)cap * 2u * sizeof(u32));
    g_ld_sym  = (LpoDagSym *)malloc((size_t)cap * 2u * sizeof(LpoDagSym));
    u8 ok = (g_ld_node != NULL && g_ld_kids != NULL &&
             g_ld_tab != NULL && g_ld_sym != NULL);
    for (u32 s = 0; s < 2u; s++) {
      g_ld_cls[s]     = (u32 *)malloc((size_t)cap * sizeof(u32));
      g_ld_occ_beg[s] = (u32 *)malloc(((size_t)cap + 1u) * sizeof(u32));
      g_ld_occ_pos[s] = (u32 *)malloc((size_t)cap * sizeof(u32));
      ok = (u8)(ok && g_ld_cls[s] != NULL && g_ld_occ_beg[s] != NULL &&
                g_ld_occ_pos[s] != NULL);
    }
    if (!ok) {
      lpo_dag_free();
      return 0u;
    }
    g_ld_cap = cap;
  }
  u32 t = 64u;
  while (t < 2u * n) {
    t <<= 1;
  }
  g_ld_tab_mask = t - 1u;
  memset(g_ld_tab, 0, (size_t)t * sizeof(u32));
  memset(g_ld_sym, 0, (size_t)t * sizeof(LpoDagSym));
  return 1u;
}

static inline u32 lpo_dag_hash(u64 h) {
  h *= 0x9E3779B97F4A7C15ull;
  h ^= h >> 29;
  h *= 0xBF58476D1CE4E5B9ull;
  h ^= h >> 32;
  return (u32)h;
}

// Record (sym -> w, arity); 0 if sym was already seen with another.
static u8 lpo_dag_sym_ok(i32 sym, i32 w, u32 arity) {
  u32 slot = lpo_dag_hash((u64)(u32)sym) & g_ld_tab_mask;
  for (;;) {
    LpoDagSym *e = &g_ld_sym[slot];
    if (!e->used) {
      e->used  = 1u;
      e->sym   = sym;
      e->w     = w;
      e->arity = arity;
      return 1u;
    }
    if (e->sym == sym) {
      return (u8)(e->w == w && e->arity == arity);
    }
    slot = (slot + 1u) & g_ld_tab_mask;
  }
}

// Hash-cons one side into the DAG, children before parents (reverse
// preorder), recording each position's class in g_ld_cls[side].
static u8 lpo_dag_side(u32 side, const KboFlatNode *x, u32 n,
                       const LpoConfig *cfg) {
  u32 *cls = g_ld_cls[side];
  for (u32 i = n; i-- > 0u; ) {
    const KboFlatNode *nd  = &x[i];
    u32               *kv  = &g_ld_kids[g_ld_nkids];
    u32                ar  = 0u;
    u32                end = i + nd->sz;
    for (u32 c = i + 1u; c < end; c += x[c].sz) {
      kv[ar++] = cls[c];
    }
    if (nd->sym >= 0 && !lpo_dag_sym_ok(nd->sym, nd->w, ar)) {
      return 0u;
    }
    u64 h = ((u64)(u32)nd->sym << 32) ^ (u64)(u32)nd->w ^ ((u64)ar << 56);
    for (u32 k = 0; k < ar; k++) {
      h = (u64)lpo_dag_hash(h ^ ((u64)kv[k] << 20)) ^ (h << 7);
    }
    u32 slot = lpo_dag_hash(h) & g_ld_tab_mask;
    u32 c    = 0u;
    for (;;) {
      u32 e = g_ld_tab[slot];
      if (e == 0u) {
        c = g_ld_n++;
        g_ld_tab[slot] = c + 1u;
        LpoDagNode *q = &g_ld_node[c];
        q->sym   = nd->sym;
        q->w     = nd->w;
        q->sz    = nd->sz;
        q->arity = ar;
        q->kids  = g_ld_nkids;
        q->rep   = (side << 31) | i;
        g_ld_nkids += ar;
        if (nd->sym < 0) {
          u32 v = (u32)(-nd->sym - 1);
          q->p      = 0u;
          q->maxp   = 0u;
          q->hasvar = 1u;
          q->vmask  = (v < 64u) ? ((u64)1 << v) : 0u;
          q->bigvar = (u8)(v >= 64u);
        } else {
          u32 lab = (u32)nd->sym;
          q->p      = (lab < cfg->n_labels) ? cfg->precedence[lab] : 0u;
          q->maxp   = q->p;
          q->hasvar = 0u;
          q->vmask  = 0u;
          q->bigvar = 0u;
          for (u32 k = 0; k < ar; k++) {
            const LpoDagNode *kn = &g_ld_node[kv[k]];
            if (kn->maxp > q->maxp) {
              q->maxp = kn->maxp;
            }
            q->vmask  |= kn->vmask;
            q->hasvar |= kn->hasvar;
            q->bigvar |= kn->bigvar;
          }
        }
        break;
      }
      const LpoDagNode *q = &g_ld_node[e - 1u];
      if (q->sym == nd->sym && q->w == nd->w && q->arity == ar &&
          memcmp(&g_ld_kids[q->kids], kv, (size_t)ar * sizeof(u32)) == 0) {
        c = e - 1u;
        break;
      }
      slot = (slot + 1u) & g_ld_tab_mask;
    }
    cls[i] = c;
  }
  return 1u;
}

static inline u32 lpo_dag_memo_slot(u32 c1, u32 c2) {
  return lpo_dag_hash(((u64)c1 << 32) | (u64)c2) & g_ld_memo_mask;
}

// Clear `want` or more slots (a power of two) for this compare, reusing
// the table unless it is too small or a monster compare left it large.
static u8 lpo_dag_memo_init(u32 want) {
  u32 m = 256u;
  while (m < want) {
    m <<= 1;
  }
  if (m > g_ld_memo_cap || (g_ld_memo_cap > LPO_DAG_MEMO_KEEP && m <= LPO_DAG_MEMO_KEEP)) {
    free(g_ld_memo);
    g_ld_memo     = (LpoDagMemo *)malloc((size_t)m * sizeof(LpoDagMemo));
    g_ld_memo_cap = (g_ld_memo != NULL) ? m : 0u;
    if (g_ld_memo == NULL) {
      return 0u;
    }
  }
  g_ld_memo_cnt  = 0u;
  g_ld_memo_full = 0u;
  g_ld_memo_mask = m - 1u;
  memset(g_ld_memo, 0, (size_t)m * sizeof(LpoDagMemo));
  return 1u;
}

// Double the slots this compare uses; past LPO_DAG_MEMO_MAX (or on an
// allocation failure) the memo stops taking entries -- a missing entry
// is recomputed, never wrong.
static void lpo_dag_memo_grow(void) {
  u32 old_n = g_ld_memo_mask + 1u;
  u32 new_n = old_n << 1;
  if (new_n > LPO_DAG_MEMO_MAX) {
    g_ld_memo_full = 1u;
    return;
  }
  LpoDagMemo *nm = (LpoDagMemo *)calloc(new_n, sizeof(LpoDagMemo));
  if (nm == NULL) {
    g_ld_memo_full = 1u;
    return;
  }
  LpoDagMemo *old = g_ld_memo;
  g_ld_memo      = nm;
  g_ld_memo_cap  = new_n;
  g_ld_memo_mask = new_n - 1u;
  for (u32 i = 0; i < old_n; i++) {
    if (old[i].k1 != 0u) {
      u32 s = lpo_dag_memo_slot(old[i].k1 - 1u, old[i].k2);
      while (nm[s].k1 != 0u) {
        s = (s + 1u) & g_ld_memo_mask;
      }
      nm[s] = old[i];
    }
  }
  free(old);
}

static void lpo_dag_memo_put(u32 c1, u32 c2, LpoCmp r) {
  if (2u * (g_ld_memo_cnt + 1u) > g_ld_memo_mask + 1u) {
    lpo_dag_memo_grow();
  }
  if (g_ld_memo_full) {
    return;
  }
  u32 s = lpo_dag_memo_slot(c1, c2);
  while (g_ld_memo[s].k1 != 0u) {
    s = (s + 1u) & g_ld_memo_mask;
  }
  g_ld_memo[s].k1 = c1 + 1u;
  g_ld_memo[s].k2 = c2;
  g_ld_memo[s].v  = (u32)(u8)(i8)r;
  g_ld_memo_cnt++;
}

// Positions of every class on one side in increasing order, bucketed by
// class (counting sort); built on the first strict-subterm Vortest.
static void lpo_dag_occ(u32 side) {
  u32       *beg = g_ld_occ_beg[side];
  u32       *pos = g_ld_occ_pos[side];
  const u32 *cls = g_ld_cls[side];
  u32        n   = g_ld_len[side];
  memset(beg, 0, ((size_t)g_ld_n + 1u) * sizeof(u32));
  for (u32 i = 0; i < n; i++) {
    beg[cls[i] + 1u]++;
  }
  for (u32 c = 0; c < g_ld_n; c++) {
    beg[c + 1u] += beg[c];
  }
  for (u32 i = 0; i < n; i++) {
    pos[beg[cls[i]]++] = i;
  }
  for (u32 c = g_ld_n; c > 0u; c--) {
    beg[c] = beg[c - 1u];
  }
  beg[0] = 0u;
  g_ld_occ_ok[side] = 1u;
}

// 1 iff class `small` occurs strictly inside class `big`.  Any
// occurrence of `big` has the same content, so its representative's
// slice is searched.
static u8 lpo_dag_sub(u32 big, u32 small) {
  u32 rep  = g_ld_node[big].rep;
  u32 side = rep >> 31;
  u32 at   = rep & 0x7FFFFFFFu;
  if (!g_ld_occ_ok[side]) {
    lpo_dag_occ(side);
  }
  const u32 *beg = g_ld_occ_beg[side];
  const u32 *pos = g_ld_occ_pos[side];
  u32 lo = beg[small];
  u32 hi = beg[small + 1u];
  u32 e  = hi;
  while (lo < hi) {
    u32 mid = lo + ((hi - lo) >> 1);
    if (pos[mid] <= at) {
      lo = mid + 1u;
    } else {
      hi = mid;
    }
  }
  return (u8)(lo < e && pos[lo] < at + g_ld_node[big].sz);
}

// lpo_flat_var_occurs over a class.
static u8 lpo_dag_var_in(u32 c, i32 vsym) {
  const LpoDagNode *q = &g_ld_node[c];
  u32 v = (u32)(-vsym - 1);
  if (v < 64u) {
    return (u8)((q->vmask >> v) & 1u);
  }
  if (!q->bigvar) {
    return 0u;
  }
  const KboFlatNode *x  = g_ld_arr[q->rep >> 31];
  u32                at = q->rep & 0x7FFFFFFFu;
  for (u32 i = at; i < at + q->sz; i++) {
    if (x[i].sym == vsym) {
      return 1u;
    }
  }
  return 0u;
}

// lpo_pretest_groesser_flat(x, y) > 0 for non-variable roots.  Against
// a constant y, a maxp equal to y's precedence comes from y itself or
// from a symbol tied with it, so only an occurrence of y decides.
static u8 lpo_dag_vortest(u32 cx, u32 cy) {
  const LpoDagNode *x = &g_ld_node[cx];
  const LpoDagNode *y = &g_ld_node[cy];
  if (x->sz == 1u) {
    return (u8)(!y->hasvar && y->maxp < x->p);
  }
  if (y->sz == 1u) {
    if (x->maxp != y->p) {
      return (u8)(x->maxp > y->p);
    }
    return lpo_dag_sub(cx, cy);
  }
  if (x->sz <= y->sz) {
    return 0u;
  }
  return lpo_dag_sub(cx, cy);
}

// The flat machine's case analysis over class pairs, on an explicit stack:
// one frame per open lpo_dag_compute(c1, c2) of the recursion this
// replaces, `k` / `d` the kid cursors its step has reached.  A wrapped
// frame (one lpo_dag_rec opened) records its verdict in the lossless memo
// when it finishes; the root, run by lpo_dag_cmp, is unwrapped.
typedef struct {
  u32 c1;
  u32 c2;
  u32 k;
  u32 d;
  u8  wrapped;
  u8  step;
} LpoDagFrame;
enum {
  LPO_D_BEGIN,
  LPO_D_GT_DOM,     // w(x) > w(y): x > every kid of y -> GT
  LPO_D_GT_SOME,    //   else some kid of y >= x -> LT, else UN
  LPO_D_LT_DOM,     // w(x) < w(y): y > every kid of x -> LT
  LPO_D_LT_SOME,    //   else some kid of x >= y -> GT, else UN
  LPO_D_EQ_SOME_X,  // equal: some kid of x >= y -> GT
  LPO_D_EQ_SOME_Y,  //        some kid of y >= x -> LT
  LPO_D_LEX,        //        kids pairwise, the first not EQ
  LPO_D_TAIL_GT,    // lex GT: x > every kid of y from d -> GT, else UN
  LPO_D_TAIL_LT     // lex LT: y > every kid of x from d -> LT, else UN
};

static LpoCmp lpo_dag_compute(u32 c1, u32 c2) {
  LpoDagFrame  fr_inl[LPO_WALK_INL];
  LpoDagFrame *fr  = fr_inl;
  u32          nfr = 1u;
  u32          cfr = LPO_WALK_INL;
  fr[0].c1      = c1;
  fr[0].c2      = c2;
  fr[0].k       = 0u;
  fr[0].d       = 0u;
  fr[0].wrapped = 0u;
  fr[0].step    = LPO_D_BEGIN;
  LpoCmp val  = LPO_UN;   // a call's verdict, pending for the top frame
  u8     have = 0u;
  for (;;) {
    LpoDagFrame      *f   = &fr[nfr - 1u];
    const LpoDagNode *x   = &g_ld_node[f->c1];
    const LpoDagNode *y   = &g_ld_node[f->c2];
    const u32        *kx  = &g_ld_kids[x->kids];
    const u32        *ky  = &g_ld_kids[y->kids];
    u32               nx  = x->arity;
    u32               ny  = y->arity;
    u8                fin = 0u;
    LpoCmp            res = LPO_UN;
    if (have) {
      have = 0u;
      switch (f->step) {
        case LPO_D_GT_DOM:
          if (val == LPO_GT) {
            f->k++;
          } else {
            f->step = LPO_D_GT_SOME;
            f->k    = 0u;
          }
          break;
        case LPO_D_LT_DOM:
          if (val == LPO_GT) {
            f->k++;
          } else {
            f->step = LPO_D_LT_SOME;
            f->k    = 0u;
          }
          break;
        case LPO_D_GT_SOME:
        case LPO_D_EQ_SOME_Y:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_LT;
          } else {
            f->k++;
          }
          break;
        case LPO_D_LT_SOME:
        case LPO_D_EQ_SOME_X:
          if (val == LPO_EQ || val == LPO_GT) {
            fin = 1u;
            res = LPO_GT;
          } else {
            f->k++;
          }
          break;
        case LPO_D_LEX:
          if (val == LPO_EQ) {
            f->d++;
          } else if (val == LPO_GT) {
            f->step = LPO_D_TAIL_GT;
            f->k    = f->d;
          } else if (val == LPO_LT) {
            f->step = LPO_D_TAIL_LT;
            f->k    = f->d;
          } else {
            fin = 1u;
            res = LPO_UN;
          }
          break;
        default:   // LPO_D_TAIL_GT, LPO_D_TAIL_LT
          if (val != LPO_GT) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->k++;
          }
          break;
      }
    }
    // The next call lpo_dag_rec(p, q), or the verdict.
    u32 p    = 0u;
    u32 q    = 0u;
    u8  call = 0u;
    while (!fin && !call) {
      switch (f->step) {
        case LPO_D_BEGIN: {
          if (f->c1 == f->c2) {
            fin = 1u;
            res = LPO_EQ;
            break;
          }
          u8 xv = (u8)(x->sym < 0);
          u8 yv = (u8)(y->sym < 0);
          if (xv | yv) {
            fin = 1u;
            if (xv && yv) {
              res = LPO_UN;
            } else if (xv) {
              res = lpo_dag_var_in(f->c2, x->sym) ? LPO_LT : LPO_UN;
            } else {
              res = lpo_dag_var_in(f->c1, y->sym) ? LPO_GT : LPO_UN;
            }
            break;
          }
          if (lpo_dag_vortest(f->c1, f->c2)) {
            fin = 1u;
            res = LPO_GT;
            break;
          }
          if (lpo_dag_vortest(f->c2, f->c1)) {
            fin = 1u;
            res = LPO_LT;
            break;
          }
          f->k = 0u;
          if (x->w > y->w) {
            f->step = LPO_D_GT_DOM;
          } else if (x->w < y->w) {
            f->step = LPO_D_LT_DOM;
          } else {
            f->step = LPO_D_EQ_SOME_X;
          }
          break;
        }
        case LPO_D_GT_DOM:
          if (f->k < ny) {
            p = f->c1; q = ky[f->k];
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_GT;
          }
          break;
        case LPO_D_GT_SOME:
          if (f->k < ny) {
            p = ky[f->k]; q = f->c1;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_UN;
          }
          break;
        case LPO_D_LT_DOM:
          if (f->k < nx) {
            p = f->c2; q = kx[f->k];
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_LT;
          }
          break;
        case LPO_D_LT_SOME:
          if (f->k < nx) {
            p = kx[f->k]; q = f->c2;
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_UN;
          }
          break;
        case LPO_D_EQ_SOME_X:
          if (f->k < nx) {
            p = kx[f->k]; q = f->c2;
            call = 1u;
          } else {
            f->step = LPO_D_EQ_SOME_Y;
            f->k    = 0u;
          }
          break;
        case LPO_D_EQ_SOME_Y:
          if (f->k < ny) {
            p = ky[f->k]; q = f->c1;
            call = 1u;
          } else if (x->sym != y->sym) {
            fin = 1u;
            res = LPO_UN;
          } else {
            f->step = LPO_D_LEX;
            f->d    = 0u;
          }
          break;
        case LPO_D_LEX:
          if (f->d < ((nx < ny) ? nx : ny)) {
            p = kx[f->d]; q = ky[f->d];
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_EQ;
          }
          break;
        case LPO_D_TAIL_GT:
          if (f->k < ny) {
            p = f->c1; q = ky[f->k];
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_GT;
          }
          break;
        default:   // LPO_D_TAIL_LT
          if (f->k < nx) {
            p = f->c2; q = kx[f->k];
            call = 1u;
          } else {
            fin = 1u;
            res = LPO_LT;
          }
          break;
      }
    }
    if (call) {
      // lpo_dag_rec(p, q): the memoized wrapper.
      g_lpo_flat_rec_calls++;
      if (p == q) {
        val  = LPO_EQ;
        have = 1u;
        continue;
      }
      if (lpo_deadline_fired()) {
        val  = LPO_UN;
        have = 1u;
        continue;
      }
      u32 sl  = lpo_dag_memo_slot(p, q);
      u8  hit = 0u;
      while (g_ld_memo[sl].k1 != 0u) {
        if (g_ld_memo[sl].k1 == p + 1u && g_ld_memo[sl].k2 == q) {
          g_lpo_flat_memo_hits++;
          val = (LpoCmp)(i8)(u8)g_ld_memo[sl].v;
          hit = 1u;
          break;
        }
        sl = (sl + 1u) & g_ld_memo_mask;
      }
      if (hit) {
        have = 1u;
        continue;
      }
      g_lpo_flat_compute_calls++;
      if (nfr == cfr) {
        fr = (LpoDagFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      LpoDagFrame *nf = &fr[nfr++];
      nf->c1      = p;
      nf->c2      = q;
      nf->k       = 0u;
      nf->d       = 0u;
      nf->wrapped = 1u;
      nf->step    = LPO_D_BEGIN;
      continue;
    }
    if (f->wrapped) {
      lpo_dag_memo_put(f->c1, f->c2, res);
    }
    if (--nfr == 0u) {
      term_stack_release(fr, fr_inl);
      return res;
    }
    val  = res;
    have = 1u;
  }
}

// Hash-cons g_lpo_flat_a[0, na) and g_lpo_flat_b[0, nb).  0 when a
// symbol has two arities or precedences, or on allocation failure.
static u8 lpo_dag_build(u32 na, u32 nb, const LpoConfig *cfg) {
  if (!lpo_dag_reserve(na + nb)) {
    return 0u;
  }
  g_ld_n         = 0u;
  g_ld_nkids     = 0u;
  g_ld_arr[0]    = g_lpo_flat_a;
  g_ld_arr[1]    = g_lpo_flat_b;
  g_ld_len[0]    = na;
  g_ld_len[1]    = nb;
  g_ld_occ_ok[0] = 0u;
  g_ld_occ_ok[1] = 0u;
  if (!lpo_dag_side(0u, g_lpo_flat_a, na, cfg) ||
      !lpo_dag_side(1u, g_lpo_flat_b, nb, cfg)) {
    return 0u;
  }
  return lpo_dag_memo_init(4u * g_ld_n);
}

// The DAG verdict on the encoded pair; the flat recursion when the DAG
// cannot represent the pair (a symbol with two arities) or allocate.
static LpoCmp lpo_dag_cmp(u32 na, u32 nb, const LpoConfig *cfg) {
  if (!lpo_dag_build(na, nb, cfg)) {
    g_lpo_dag_fallbacks++;
    return lpo_flat_ref(cfg);
  }
  return lpo_dag_compute(g_ld_cls[0][0], g_ld_cls[1][0]);
}

// Pairs of at most LPO_DAG_MIN_NODES nodes stay on the flat recursion:
// there hash-consing costs more than the recursion it saves (on RNG
// runs the DAG took 1.4x the flat time up to 16 nodes and 1.2x up to
// 32, and 0.8x down to 0.07x above).  The self-check build sends every
// pair through the DAG so the differential covers all sizes.
#ifdef THVM_LPO_SELFCHECK
#define LPO_DAG_MIN_NODES 0u
#else
#define LPO_DAG_MIN_NODES 32u
#endif

static LpoCmp lpo_flat_cmp_run(u32 na, u32 nb, const LpoConfig *cfg) {
  g_lpo_top_calls++;
  g_lpo_flat_compute_calls++;
  if (na + nb <= LPO_DAG_MIN_NODES) {
    return lpo_flat_ref(cfg);
  }
  return lpo_dag_cmp(na, nb, cfg);
}

#ifdef THVM_LPO_SELFCHECK
#include <time.h>

// Differential self-check (-DTHVM_LPO_SELFCHECK): every top-level flat
// compare also runs lpo_flat_ref and counts disagreements (both are
// timed), and runs the reversed compare on the DAG, counting verdicts
// that are not its inverse.  A compare whose deadline fires is not
// counted.  With
// THVM_LPO_SELFCHECK_DUMP=<file>, a pair on which the DAG computes at
// least THVM_LPO_SELFCHECK_DUMP_MIN (default 2000) class pairs, or any
// mismatch, is appended to <file> BEFORE the reference runs, so pairs
// the reference cannot finish are still checked offline
// (bin/test_lpo <file>).
static u64 g_lpo_sc_pairs    = 0;
static u64 g_lpo_sc_mismatch = 0;
static u64 g_lpo_sc_skipped  = 0;
static u64 g_lpo_sc_dumped   = 0;
static u64 g_lpo_sc_ns_new   = 0;
static u64 g_lpo_sc_ns_ref   = 0;
static u64 g_lpo_sc_max_ref  = 0;
static u64 g_lpo_sc_max_new  = 0;   // the DAG's time on the max_ref pair
static u32 g_lpo_sc_max_n    = 0;   // na + nb of the max_ref pair
static u64 g_lpo_sc_max_dag  = 0;   // slowest DAG compare
static u32 g_lpo_sc_max_dag_n = 0;
// Antisymmetry of the top-level verdicts: the reversed compare on the
// same DAG must return the inverse verdict (EQ and UN to themselves, GT
// to LT).
static u64 g_lpo_sc_anti_pairs = 0;
static u64 g_lpo_sc_anti_bad   = 0;
static u8  g_lpo_sc_reg      = 0;
// Per-size buckets (na + nb <= 16, 32, ..., 2^15, more): pairs and the
// time each evaluator spent on them.
#define LPO_SC_NB 13
static u64 g_lpo_sc_b_n[LPO_SC_NB];
static u64 g_lpo_sc_b_new[LPO_SC_NB];
static u64 g_lpo_sc_b_ref[LPO_SC_NB];

static u32 lpo_sc_bucket(u32 n) {
  u32 b = 0u;
  while (b + 1u < LPO_SC_NB && n > (16u << b)) {
    b++;
  }
  return b;
}

static u64 lpo_sc_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

fn void thvm_lpo_selfcheck_report(FILE *f) {
  fprintf(f, "   lpo-selfcheck: pairs=%llu mismatches=%llu skipped=%llu "
             "dumped=%llu fallbacks=%llu new=%.3fs ref=%.3fs "
             "max-ref=%.3fms (n=%u, new %.3fms) max-new=%.3fms (n=%u)\n",
          (unsigned long long)g_lpo_sc_pairs,
          (unsigned long long)g_lpo_sc_mismatch,
          (unsigned long long)g_lpo_sc_skipped,
          (unsigned long long)g_lpo_sc_dumped,
          (unsigned long long)g_lpo_dag_fallbacks,
          (double)g_lpo_sc_ns_new / 1e9, (double)g_lpo_sc_ns_ref / 1e9,
          (double)g_lpo_sc_max_ref / 1e6, g_lpo_sc_max_n,
          (double)g_lpo_sc_max_new / 1e6,
          (double)g_lpo_sc_max_dag / 1e6, g_lpo_sc_max_dag_n);
  fprintf(f, "   lpo-selfcheck-antisym: pairs=%llu violations=%llu\n",
          (unsigned long long)g_lpo_sc_anti_pairs,
          (unsigned long long)g_lpo_sc_anti_bad);
  fprintf(f, "   lpo-selfcheck-sizes:");
  for (u32 b = 0; b < LPO_SC_NB; b++) {
    if (g_lpo_sc_b_n[b] != 0u) {
      fprintf(f, " <=%u:%llu/%.0f/%.0f", 16u << b,
              (unsigned long long)g_lpo_sc_b_n[b],
              (double)g_lpo_sc_b_new[b] / (double)g_lpo_sc_b_n[b],
              (double)g_lpo_sc_b_ref[b] / (double)g_lpo_sc_b_n[b]);
    }
  }
  fprintf(f, " (pairs/new-ns/ref-ns per pair)\n");
}

static void lpo_sc_atexit(void) {
  thvm_lpo_selfcheck_report(stderr);
}

// Record: "LPOP", na, nb, n_labels, precedence[n_labels], then na + nb
// nodes (sym, w, sz), then the DAG verdict as i32.
static void lpo_sc_dump(u32 na, u32 nb, const LpoConfig *cfg, LpoCmp r) {
  static FILE *f   = NULL;
  static int   off = 0;
  if (off) {
    return;
  }
  if (f == NULL) {
    const char *p = getenv("THVM_LPO_SELFCHECK_DUMP");
    f = (p != NULL && p[0] != '\0') ? fopen(p, "ab") : NULL;
    if (f == NULL) {
      off = 1;
      return;
    }
  }
  u32 hdr[4] = {0x504F504Cu, na, nb, cfg->n_labels};
  i32 rv     = (i32)r;
  fwrite(hdr, sizeof(u32), 4, f);
  fwrite(cfg->precedence, sizeof(u32), cfg->n_labels, f);
  fwrite(g_lpo_flat_a, sizeof(KboFlatNode), na, f);
  fwrite(g_lpo_flat_b, sizeof(KboFlatNode), nb, f);
  fwrite(&rv, sizeof(i32), 1, f);
  fflush(f);
  g_lpo_sc_dumped++;
}

static LpoCmp lpo_flat_cmp(u32 na, u32 nb, const LpoConfig *cfg) {
  static i64 dump_min = -1;
  if (!g_lpo_sc_reg) {
    g_lpo_sc_reg = 1u;
    atexit(lpo_sc_atexit);
  }
  if (dump_min < 0) {
    const char *e = getenv("THVM_LPO_SELFCHECK_DUMP_MIN");
    dump_min = (e != NULL && e[0] != '\0') ? atoll(e) : 2000;
  }
  u64 c0 = g_lpo_flat_compute_calls;
  u64 t0 = lpo_sc_now();
  LpoCmp r = lpo_flat_cmp_run(na, nb, cfg);
  u64 t1 = lpo_sc_now();
  if (g_lpo_past_deadline) {
    g_lpo_sc_skipped++;
    return r;
  }
  if (t1 - t0 > g_lpo_sc_max_dag) {
    g_lpo_sc_max_dag   = t1 - t0;
    g_lpo_sc_max_dag_n = na + nb;
  }
  u8 dumped = 0u;
  if ((i64)(g_lpo_flat_compute_calls - c0) >= dump_min) {
    lpo_sc_dump(na, nb, cfg, r);
    dumped = 1u;
  }
  u64 sr = g_lpo_flat_rec_calls, sh = g_lpo_flat_memo_hits;
  u64 sc = g_lpo_flat_compute_calls;
  LpoCmp ref = lpo_flat_ref(cfg);
  u64 t2 = lpo_sc_now();
  g_lpo_flat_rec_calls     = sr;
  g_lpo_flat_memo_hits     = sh;
  g_lpo_flat_compute_calls = sc;
  if (g_lpo_past_deadline) {
    g_lpo_sc_skipped++;
    return r;
  }
  g_lpo_sc_pairs++;
  g_lpo_sc_ns_new += t1 - t0;
  g_lpo_sc_ns_ref += t2 - t1;
  {
    u32 b = lpo_sc_bucket(na + nb);
    g_lpo_sc_b_n[b]++;
    g_lpo_sc_b_new[b] += t1 - t0;
    g_lpo_sc_b_ref[b] += t2 - t1;
  }
  if (t2 - t1 > g_lpo_sc_max_ref) {
    g_lpo_sc_max_ref = t2 - t1;
    g_lpo_sc_max_new = t1 - t0;
    g_lpo_sc_max_n   = na + nb;
  }
  // The reversed compare on the same DAG (fresh memo).
  if (lpo_dag_build(na, nb, cfg)) {
    u64 sr2 = g_lpo_flat_rec_calls, sh2 = g_lpo_flat_memo_hits;
    u64 sc2 = g_lpo_flat_compute_calls;
    LpoCmp rev = lpo_dag_compute(g_ld_cls[1][0], g_ld_cls[0][0]);
    g_lpo_flat_rec_calls     = sr2;
    g_lpo_flat_memo_hits     = sh2;
    g_lpo_flat_compute_calls = sc2;
    if (!g_lpo_past_deadline) {
      LpoCmp inv = (r == LPO_GT) ? LPO_LT : (r == LPO_LT) ? LPO_GT : r;
      g_lpo_sc_anti_pairs++;
      if (rev != inv) {
        g_lpo_sc_anti_bad++;
      }
    }
  }
  if (ref != r) {
    g_lpo_sc_mismatch++;
    if (!dumped) {
      lpo_sc_dump(na, nb, cfg, r);
    }
    if (g_lpo_sc_mismatch <= 20u) {
      fprintf(stderr, "LPO-SELFCHECK MISMATCH na=%u nb=%u dag=%d ref=%d\n",
              na, nb, (int)r, (int)ref);
    }
  }
  return r;
}
#else
static inline LpoCmp lpo_flat_cmp(u32 na, u32 nb, const LpoConfig *cfg) {
  return lpo_flat_cmp_run(na, nb, cfg);
}
#endif

static int lpo_pretest_flat_dispatch(Term s, Term t,
                                     const LpoConfig *cfg, LpoCmp *out) {
  // Borrow KBO's encoder -- LPO does not care about weights, only sym/sz.
  // Use a stub KboConfig (var_weight=0, n_labels=0) so kbo_flat_encode
  // never tries to read an LpoConfig label array.
  static u32 stub_w[1] = {0u};
  KboConfig stub = { .weights = stub_w, .precedence = stub_w,
                     .n_labels = 0u, .var_weight = 0u };
  u32 na = 0u, nb = 0u;
  if (!kbo_flat_encode(s, &stub, g_lpo_flat_a, &na)) return 0;
  if (!kbo_flat_encode(t, &stub, g_lpo_flat_b, &nb)) return 0;
  int r1 = lpo_pretest_groesser_flat(g_lpo_flat_a, na,
                                     g_lpo_flat_b, nb, cfg);
  if (r1 > 0) { *out = LPO_GT; return 1; }
  int r2 = lpo_pretest_groesser_flat(g_lpo_flat_b, nb,
                                     g_lpo_flat_a, na, cfg);
  if (r2 > 0) { *out = LPO_LT; return 1; }
  // Both directions returned non-GT.  If BOTH ruled the strict-dominance
  // out forcibly (-1 / -1) AND neither side is var-only, the terms are
  // LPO_UN.  But the constant-side cases already covered the var-only
  // legs, and the general-scan -1 path means "no subterm hit found", which
  // is NOT a strict UN proof -- there might still be a non-subterm-based
  // LPO_GT/LT via the recursive case (2) precedence path.  So only commit
  // to UN when BOTH legs came back from the const/var branches.
  return 0;
}

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg) {
  if (g_lpo_persist) {
    // Persist across calls; invalidate only on a config change (distinct
    // precedence -> distinct verdicts) -- the caller handles cell moves.
    if (cfg != g_lpo_last_cfg) { g_lpo_last_cfg = cfg; thvm_lpo_invalidate(); }
  } else {
    // Safe default: fresh memo every call.
    thvm_lpo_invalidate();
  }
  // lpo_eq is a Term-tree walk -- redundant on the flatrec path since
  // slice_eq at the top of the flat walk's compute catches the
  // EQ case faster (it operates on the cache-dense KboFlatNode spine
  // instead of dereferencing heap cells).  Skip the early-eq check
  // when flatrec is on.
  // Flatterm subterm pretest (LV_VortestLPOGroesser).  Catches the two
  // common conclusive cases (b strict subterm of a, or a strict subterm of
  // b) in O(|s|+|t|) without recursion or memo; only the residual hard
  // cases reach lpo_rec.  Opt-in via THVM_LPO_VORTEST_FLAT until A/B
  // confirms no regression on KBO-mode or non-Sheffer benches.
  static int vortest_gate = -1;
  if (vortest_gate < 0) {
    const char *e = getenv("THVM_LPO_VORTEST_FLAT");
    vortest_gate = (e != NULL && e[0] == '1') ? 1 : 0;
  }
  LpoCmp pre;
  // Flatrec is default-on as of iter 151: the per-level Vortest +
  // direct-subterm-scan + WM-AlleDiff stack delivered +60% measured
  // throughput on AndAssoc LPO+Mix+Wald with no regressions across
  // test_lpo (12/12) and test_atp (135624/135624).  THVM_LPO_FLAT_REC=0
  // disables for differential or fallback debugging.
  static int flatrec_gate = -1;
  if (flatrec_gate < 0) {
    const char *e = getenv("THVM_LPO_FLAT_REC");
    flatrec_gate = (e != NULL && e[0] == '0') ? 0 : 1;
  }
  // When Flatrec is on, the Vortest pretest reuses the same flat encode.
  // When Flatrec is off, Vortest pays its own encode via the dispatch helper.
  if (vortest_gate && !flatrec_gate &&
      lpo_pretest_flat_dispatch(s, t, cfg, &pre)) return pre;
  // Flatterm recursive LPO (WM LPO.c port).  Encodes both operands into
  // cache-dense KboFlatNode arrays and runs the same Dershowitz LPO body
  // over them with O(1) child stepping (cur += node[cur].sz).  Memoized
  // per call on (pa, pb, side) positions.  Opt-in via THVM_LPO_FLAT_REC;
  // falls back to the Term-tree lpo_rec on encode overflow or when disabled.
  if (flatrec_gate) {
    // Top-level memo across calls keyed by (s, t).  When ATP turns on
    // persistence the engine invalidates this on every GC, mirroring
    // g_lpo_memo.  Hits return without paying the encode/recursion cost.
    if (g_lpo_persist) {
      u32 idx = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                    ^ (u64)t * 0xBF58476D1CE4E5B9ull) & LPO_MEMO_MASK;
      LpoMemoEnt *e2 = &g_lpo_memo[idx];
      if (e2->epoch == g_lpo_epoch && e2->s == s && e2->t == t)
        return (LpoCmp)(i8)e2->cmp;
    }
    u32 na = 0u, nb = 0u;
    if (lpo_flat_encode_rec(s, cfg, g_lpo_flat_a, &na) &&
        lpo_flat_encode_rec(t, cfg, g_lpo_flat_b, &nb)) {
      // Vortest reuses the encode (zero extra cost).
      if (vortest_gate) {
        int r1 = lpo_pretest_groesser_flat(g_lpo_flat_a, na,
                                           g_lpo_flat_b, nb, cfg);
        if (r1 > 0) return LPO_GT;
        int r2 = lpo_pretest_groesser_flat(g_lpo_flat_b, nb,
                                           g_lpo_flat_a, na, cfg);
        if (r2 > 0) return LPO_LT;
      }
      // Fast var-set incomparability check directly on the flat spines.
      // Avoids the lpo_var_set_acc / lpo_vars_in_range walk over
      // Term trees that the pre-flatrec lpo_pretest_varset would do.
      u32 vs[LPO_MAX_VAR] = {0};
      u32 vt[LPO_MAX_VAR] = {0};
      u8 in_range = 1u;
      for (u32 i = 0; i < na; i++) {
        if (g_lpo_flat_a[i].sym < 0) {
          u32 vid = (u32)(-g_lpo_flat_a[i].sym - 1);
          if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
          vs[vid] = 1u;
        }
      }
      if (in_range) for (u32 i = 0; i < nb; i++) {
        if (g_lpo_flat_b[i].sym < 0) {
          u32 vid = (u32)(-g_lpo_flat_b[i].sym - 1);
          if (vid >= LPO_MAX_VAR) { in_range = 0u; break; }
          vt[vid] = 1u;
        }
      }
      if (in_range) {
        u8 s_extra = 0u, t_extra = 0u;
        for (u32 i = 0; i < LPO_MAX_VAR; i++) {
          if (vs[i] && !vt[i]) s_extra = 1u;
          if (vt[i] && !vs[i]) t_extra = 1u;
        }
        if (s_extra && t_extra) return LPO_UN;
      }
      LpoCmp rr = lpo_flat_cmp(na, nb, cfg);
      if (g_lpo_persist) {
        u32 idx2 = (u32)((u64)s * 0x9E3779B97F4A7C15ull
                       ^ (u64)t * 0xBF58476D1CE4E5B9ull) & LPO_MEMO_MASK;
        LpoMemoEnt *e3 = &g_lpo_memo[idx2];
        CELL_MEMO_WRITE();
        e3->s = s; e3->t = t; e3->epoch = g_lpo_epoch; e3->cmp = (u8)rr;
      }
      return rr;
    }
  }
  // Term-tree fallback (encode overflow or flatrec disabled):
  // lpo_pretest_varset is the only remaining safety net for the UN case.
  LpoCmp pre2;
  if (lpo_pretest_varset(s, t, &pre2)) return pre2;
  return lpo_rec(s, t, cfg);
}

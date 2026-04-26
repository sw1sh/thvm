# KBO as a pure IC program: design memo (stage 8.2a)

> Sibling of `waldmeister_ic_atp.md`, `connectedness_design.md`,
> `sup_encoded_cps.md`.  Decision document for stage 8.2's
> incremental port of `thvm_kbo` from C to IC.

## Goal

Stage 8.2 calls for a pure-IC implementation of the
**Knuth-Bendix ordering** (KBO).  The C reference is in
`src/kbo/_.c` (140 lines): `thvm_kbo(s, t, cfg)` returns
`KBO_EQ / GT / LT / UN` based on Baader-Nipkow's algorithm
(domination check, weight, top-symbol precedence, lexicographic
recursion).  Today the saturation loop calls this directly from
C inside `thvm_atp_orient_and_add`.

The motivation for an IC port comes from stage 8.10 (SupGen-style
search inside saturation).  When we superpose the *choice* of
next CP, we may also want to superpose alternative *orientations*
or alternative *KboConfigs* and let priority-aware collapse pick
the best.  Doing that requires KBO to be evaluable from inside an
IC reduction -- not just callable as a C function.

## Three encoding choices

| Option | Where the work happens | Cost (LOC) | Use for |
|---|---|---|---|
| **(1) TAG_PRI wrapper** | C-internal; IC calls into C via APP-PRI | ~50 | Lets IC code invoke the existing C comparator; minimum useful increment |
| **(2) Hybrid IC + C primitives** | Structural recursion in IC; arithmetic + small primitives in C | ~200 | Demonstrates IC-native control flow on top of cheap C arithmetic; intermediate research step |
| **(3) Full pure IC** | Everything in IC; Church-numeral or TAG_NUM-encoded weights, IC-encoded variable counts, IC-encoded comparison | ~500-1000 | Research target; lets SupGen superpose KboConfigs themselves |

Each option strictly subsumes the previous in expressiveness.
None of them changes the *result* the saturation loop sees on
correctness-relevant problems -- they all compute the same KBO
function.  The difference is *how much of the computation lives
inside IC reduction*, which determines what 8.10 can superpose
across.

## Analysis

**Option (1) -- TAG_PRI wrapper.** Mirrors 8.1c's
`prim_unify_apply`: register `thvm_kbo` at id
`ATP_PRIM_KBO = 2`, build APP-PRI evaluation chain to invoke it
from IC.  The complication is the `KboConfig*` argument: pointers
don't fit in a Term's `val` field cleanly, so we'd need a small
process-global registry of configs (analogous to the primitive
table) keyed by a u32 id.  Then the primitive takes `(s, t,
cfg_id)` (arity 3) and dispatches via the registry.

This is the **minimum useful increment** because it unblocks 8.10
to invoke KBO from inside a SUP-encoded search.  It does NOT let
8.10 superpose KboConfigs themselves -- those still flow through
a single global table.

**Option (2) -- Hybrid IC + C primitives.** The structural
recursion of `kbo_rec` (the cases on `KBO_EQ`, weight check, top-
symbol comparison, lexicographic descent) gets encoded as IC
control flow: `EQL`, `MAT`, `WHEN`, `OR`, `AND` operating on a
mix of IC values and CTR-encoded results.  Variable counting,
weight summation, and comparison primitives stay in C, exposed
as `TAG_PRI` callbacks.  IC drives, C does the arithmetic.

The structural-recursion piece would benefit from SupGen-style
search the most: alternative orderings (LPO instead of KBO, or
KBO with a different precedence) become CTR alternatives in a
SUP, and the per-rule comparison fans out across them.

**Option (3) -- Full pure IC.** Every primitive becomes an IC
program.  Variable counts via Church numerals or recursive CTR
walks; weights as TAG_NUM with IC-encoded summation via
`TAG_OP2`; comparison via IC pattern matching.  Probably 500-1000
lines, comparable in scope to the entire `src/kbo/` directory but
with very different code shape.

The benefit: SupGen can superpose absolutely anything about the
ordering (weights, precedences, KBO vs LPO vs other) and let
priority-aware collapse pick the best.  This is the **research
target** but not what we'd land in a single firing.

## Decision

**Implement (1) in 8.2b.**  This is concretely useful (unblocks
8.10), bounded in scope (~50 LOC), and ports cleanly from the
existing `prim_unify_apply` pattern.

**Implement (2) partially in 8.2c**, scoped to porting the
simplest sub-routine: `kbo_eq` (structural equality on terms).
This is the "hello world" of IC structural recursion -- no
arithmetic, just CTR / FVR pattern matching with a recursive
descent.  It's a useful proof point that a pure-IC subroutine is
viable in our codebase.

**Defer (3) to 8.2d** as research infrastructure, gated on
SupGen-style search (8.10) materializing.  Without 8.10 there is
no use case that pays for the engineering cost.

## TAG_PRI wrapper details (for 8.2b)

The KboConfig registry pattern mirrors the primitive registry:

```c
#define KBO_CFG_TABLE_CAP 16
static const KboConfig *KBO_CFG_TABLE[KBO_CFG_TABLE_CAP];
fn u32 kbo_cfg_register(u32 cfg_id, const KboConfig *cfg);
fn const KboConfig *kbo_cfg_get(u32 cfg_id);
```

The primitive itself:

```c
// arity 3: (s, t, cfg_id_NUM) -> NUM(KBO_EQ/GT/LT/UN)
static Term prim_kbo(Term *args) {
  Term s   = args[0];
  Term t   = args[1];
  Term cid = args[2];
  if (term_tag(cid) != TAG_NUM) return term_new(0, TAG_ERA, 0, 0);
  const KboConfig *cfg = kbo_cfg_get((u32)term_val(cid));
  if (cfg == NULL) return term_new(0, TAG_ERA, 0, 0);
  KboCmp r = thvm_kbo(s, t, cfg);
  return term_new(0, TAG_NUM, DT_I32, (u64)r);
}
```

Encoding KboCmp as a NUM keeps things simple; a future iteration
could return a CTR with explicit variants, but NUM works for the
immediate consumer (the saturation loop's branching logic).

Tests: cover each KboCmp outcome (EQ, GT, LT, UN) plus an
unregistered cfg_id (-> ERA).  4-6 cases.

## Pure-IC `kbo_eq` details (for 8.2c)

The C version of `kbo_eq` is 17 lines: pattern-match on tags,
recurse into CTR children, compare leaf data.  The IC version
encodes the same shape but as a `TAG_PRI` whose body builds an
`AND`/`EQL` term that the reducer evaluates.

Sketch:

```c
// arity 2: (s, t) -> NUM(0 if not equal, 1 if equal)
static Term prim_kbo_eq_ic(Term *args) {
  Term s = args[0];
  Term t = args[1];
  if (term_tag(s) != term_tag(t)) return term_new(0, TAG_NUM, DT_I32, 0);
  if (term_ext(s) != term_ext(t)) return term_new(0, TAG_NUM, DT_I32, 0);
  switch (term_tag(s)) {
    case TAG_FVR: return term_new(0, TAG_NUM, DT_I32, 1);
    case TAG_CTR: {
      // Build an AND chain: AND(child_0_eq, AND(child_1_eq, ...))
      // where each child_i_eq is APP(APP(PRI(this_id), child_i_s),
      // child_i_t).  Reducing the AND short-circuits at the first
      // FALSE.
      ...
    }
    default: return term_new(0, TAG_NUM, DT_I32, term_val(s) == term_val(t));
  }
}
```

The "build an AND chain" branch is the IC-native bit -- the
reducer fires APP-PRI evaluations in sequence to compare each
child pair, and short-circuits on the first NUM(0).  Tests would
verify correctness on terms of various shapes.

Note: this is "IC-driven control flow with C-implemented base
cases" -- closer to option (2) than option (3).  A *truly* pure
IC version would encode the recursion via lambda-term combinators
without ever calling C.  We accept the C base case as a
pragmatic shortcut.

## Verification plan

For 8.2b: parity test against direct `thvm_kbo` calls on the
group axiom test cases.

For 8.2c: parity test against direct `kbo_eq` (the static C
helper) on a battery of CTR / FVR / NUM term pairs.

For 8.2d (research): integration with 8.10's SupGen search; the
demonstration target is "superpose two KboConfigs, run the
saturation under both, observe priority-aware collapse picking
the better one."

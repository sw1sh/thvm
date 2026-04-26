# LPO ordering: design memo (stage 8.5a)

> Sibling of `kbo_ic_design.md` and `multi_sort.md`.  Decision
> document for stage 8.5's port of `Lexikografische-Pfad-Ordnung`
> (LPO, "lexicographic path ordering") alongside the existing
> Knuth-Bendix ordering (KBO).

## Goal

Stage 8.5 adds the **lexicographic path ordering** as an
alternative reduction ordering.  Waldmeister supports both --
LPO is more discriminating on certain rewriting systems
(particularly those without obvious weight functions), and is
the standard pick for many TPTP-UEQ problems.

Today our `.pr` parser accepts an `ORDERING LPO ...` directive
but the saturator silently substitutes a KBO config built from
the precedence ranks.  Most fixtures (`group_*.pr`, `monoid_*.pr`)
declare `ORDERING LPO` -- so this gap matters in practice.

## LPO algorithm (Dershowitz, 1982)

LPO is a recursive comparison built from a **precedence**
relation `>_F` on function symbols.  Variables are leaf cases;
the recursion handles CTR-vs-CTR, CTR-vs-FVR, FVR-vs-CTR.

For terms `s = f(s_1, ..., s_m)` and `t = g(t_1, ..., t_n)`:

```
s >_lpo t iff one of:
  (1) s_i >=_lpo t for some i in 1..m       (subterm domination)
  (2) f >_F g       AND s >_lpo t_j  for all j in 1..n
  (3) f == g        AND (s_1, ..., s_m) >_lex (t_1, ..., t_n)
                    AND s >_lpo t_j  for all j in 1..n
```

For variables: `s >_lpo x` iff x occurs as a strict subterm of
`s`.  `x >_lpo t` iff false.  `x >_lpo x` is false (we use `>=`
to denote `> or =`).

Returns one of `LPO_EQ / GT / LT / UN`, mirroring `KboCmp`'s
four-valued shape.

## LpoConfig

LPO needs only a **precedence** -- no weights, no `var_weight`.
The minimum config is:

```c
typedef enum {
  LPO_EQ = 0,
  LPO_GT = 1,
  LPO_LT = -1,
  LPO_UN = 2,
} LpoCmp;

typedef struct {
  const u32 *precedence;   // higher value = greater symbol
  u32        n_labels;
} LpoConfig;

fn LpoCmp thvm_lpo(Term s, Term t, const LpoConfig *cfg);
```

We could reuse `KboConfig` directly (it has `precedence` and
`n_labels`), but having a separate `LpoConfig` makes the type
signatures self-documenting.  Decision: **separate config struct**.

## Selector pattern -- KBO vs LPO

Three placement options:

### A. Sum type / discriminated union

```c
typedef enum { ORD_KIND_KBO, ORD_KIND_LPO } OrdKind;
typedef struct {
  OrdKind kind;
  union {
    const KboConfig *kbo;
    const LpoConfig *lpo;
  } u;
} OrderConfig;
```

Pro: type-safe; single `const OrderConfig *cfg` field on AtpState.
Con: every existing caller of `thvm_atp_init(&kbo_cfg, ...)`
must change to wrap into an OrderConfig.  Significant churn
across tests.

### B. Two parallel inits

```c
fn AtpState *thvm_atp_init       (const KboConfig *cfg, u32 step_cap);
fn AtpState *thvm_atp_init_lpo   (const LpoConfig *cfg, u32 step_cap);
```

Pro: zero churn for KBO callers; LPO users opt in explicitly.
Con: two ortho fields on AtpState (`const KboConfig *kbo;
const LpoConfig *lpo;`); orient logic must dispatch on which
is non-NULL.

### C. Add LPO field to AtpState alongside KBO; selector flag

```c
struct AtpState {
  ...
  const KboConfig *kbo;
  const LpoConfig *lpo;   // NEW: when non-NULL, used in place of kbo
  ...
};
```

Pro: minimal churn; existing code keeps working; LPO callers
poke the field directly (mirrors how `use_ic_cp_gen` /
`use_ic_rewrite` are direct field pokes today).
Con: invariant that exactly one of `kbo` / `lpo` is non-NULL is
not enforced by the type.

## Decision: Choice C

**Add a `const LpoConfig *lpo` field to AtpState alongside the
existing `kbo` field.**  When `lpo != NULL`, `thvm_atp_orient_
and_add` calls `thvm_lpo` instead of `thvm_kbo`.  When both are
non-NULL, LPO wins (deterministic precedence; arguably the
opposite is also reasonable).  When both are NULL, orient
returns `KBO_UN` (no ordering, every comparison is incomparable
-- the saturator falls into unfailing-completion fallback).

This matches the `use_ic_cp_gen` / `use_ic_rewrite` pattern: a
simple boolean-ish flag rather than a sum type.  Keeps existing
tests working untouched.

## Migration plan

**8.5b**: implement `thvm_lpo` in `src/lpo/_.c` mirroring the
structure of `src/kbo/_.c`.  Helper functions:
- `lpo_eq` -- structural equality (can reuse `kbo_eq` from kbo
  module since it's static-but-TU-visible; or copy locally for
  cleanliness).
- `lpo_var_in_term` -- "x occurs as a strict subterm of s"
  predicate; needed for the variable case.
- `lpo_rec` -- the recursive comparator implementing the three
  cases above.

Tests in `tests/test_lpo.c`:
- `lpo/eq-on-identical-terms`
- `lpo/gt-via-precedence`: `f(a) > b` when `f >_F b`
- `lpo/gt-via-subterm-domination`: `f(a, b) > a` directly
- `lpo/gt-via-lex-on-equal-heads`: `f(a, c) > f(a, b)` when
  `c >_F b`
- `lpo/un-on-incompatible-vars`: `x > a` and `a > x` both
  false; gives `LPO_UN`
- `lpo/var-occurs-as-subterm`: `f(x) > x` should hold

**8.5c**: extend AtpState with `const LpoConfig *lpo`.  Update
`thvm_atp_orient_and_add` to dispatch.  Add a small helper
`atp_compare(s, t, atp)` that picks KBO or LPO based on which
config is present and returns a unified `KboCmp`-shaped result
(treat LPO_EQ/GT/LT/UN as the corresponding KBO_EQ/GT/LT/UN).

**8.5d**: at least one fixture under `tests/data/atp/` should
parse and run through LPO instead of KBO.  Two paths:
1. Add a new fixture (e.g. `group_lpo.pr`) that we explicitly
   wire LPO for in the bench harness.
2. Update existing fixtures' driver: when the .pr declares
   `ORDERING LPO`, use the LPO path.

The clean answer is (2) since most fixtures already declare
`ORDERING LPO` -- the parser even captures this in
`spec->ordering_kind` (or would, if we added that field).
But (2) requires extending WaldSpec to track the chosen
ordering kind from the .pr's ORDERING section.  v0 picks (1)
to keep the change surface small; (2) is a follow-up.

## Verification

- `make test` must stay green; existing KBO callers untouched.
- `tests/test_lpo.c` covers the 6 cases above.
- A small parity assertion: on a fixture where KBO and LPO
  produce the same orientations (e.g. simple monoid axioms),
  saturation outcomes must agree under both orderings.

## Stop conditions

If 8.5b reveals that LPO's recursive comparator produces
non-trivial stack depth on our small test corpus (each call
makes m+n+1 recursive calls in the worst case), document the
performance characteristic but accept it -- the corpus terms
are small enough that even 4-deep recursion is fine.

If 8.5c shows that switching from KBO to LPO on existing
fixtures changes proof outcomes (e.g., `group_commutative_
inverse.pr` going from TIMEOUT to PROVED, or vice versa),
document the change in `docs/bench-atp.md` and pick the
default that matches the .pr file's declared ordering.

If 8.5d's fixture exposes a real correctness bug in LPO (e.g.,
non-termination under the chosen precedence), back out and
file a bug -- LPO requires a *well-founded* precedence, and a
malformed .pr could violate that.

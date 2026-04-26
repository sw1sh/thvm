# Multi-witness narrowing enumeration: design memo (stage 9.1a)

> First memo of Stage 9 (follow-on continuation).  Sibling of
> `narrowing_design.md` (8.9a) -- this is the natural extension
> of 8.9's first-witness narrow into a bounded all-witnesses
> search.

## Goal

Stage 8.9 lands a first-witness narrowing path: every successful
unification commits eagerly, accumulates the binding into
`s->witness_subst`, and proves the goal as soon as both sides
become structurally equal.  Useful for confirming that *a*
witness exists, but it doesn't enumerate all of them.

Stage 9.1 extends this with a bounded depth-first search that
collects multiple witnesses.  Two motivations:

- Some equational queries have a small finite witness set
  (e.g. group-axiom problems with two distinct unifiers); the
  user wants all of them.
- Multi-witness IS a small-scale trace search.  Implementing it
  here lays groundwork for 8.10's deferred trace-level SupGen
  research item.

## Algorithm sketch

A bounded DFS over the choice tree where:

- Each node is a `(lhs, rhs, accumulated_σ, depth)` state.
- Children are obtained by trying every (position, rule) pair
  on `lhs` then `rhs`; each successful unify makes one child.
- A leaf is reached when either:
  - `kbo_eq(lhs, rhs)` -> accept; emit `accumulated_σ` as a
    witness.
  - No (position, rule) applies -> reject this branch.
  - `depth >= max_depth` -> truncate.
  - `len(witnesses_collected) >= max_witnesses` -> stop search.

Output: array of `RewriteSubst` (one per witness), filled in
DFS order.

## Three bound parameters

| Bound | Default | Why |
|---|---|---|
| `max_depth` | 8 | mirrors `ATP_NARROW_BUDGET` from 8.9c |
| `max_witnesses` | 16 | conservative; users with more wanted bump |
| `step_cap` (saturation steps under each branch) | 0 | v0: no further saturation in the DFS, just pure narrowing on the existing R |

The third bound is interesting: ideally the search would also
extend `R` via saturation between narrow steps so that more
unifications become possible.  But that compounds complexity.
v0 keeps `R` fixed during the DFS and just enumerates over the
already-existing rule set.

## Distinctness

Two witnesses might be alpha-equivalent (same up to renaming of
non-existential FVRs) or syntactically identical.  v0 returns
**raw witnesses without dedup**.  Caller can post-filter:

```c
fn u8 atp_witness_eq(const RewriteSubst *a, const RewriteSubst *b);
```

(future helper, not part of 9.1).  Or in WL via
`DeleteDuplicates` on the result list.

## API

```c
// 9.1b: bounded DFS narrowing.  Same starting (lhs, rhs) as
// thvm_atp_narrow_step, but enumerates up to `max_witnesses`
// successful witness paths within `max_depth` steps each.
// Returns the count of witnesses found (<= max_witnesses).
// Out-param `witnesses[]` must hold at least max_witnesses
// RewriteSubst slots.
fn u32 thvm_atp_narrow_all(AtpState *s,
                           Term lhs, Term rhs,
                           u32 max_depth,
                           u32 max_witnesses,
                           RewriteSubst *witnesses);
```

The function is **stateless w.r.t. `s->witness_subst`**: it
populates the caller's array directly, leaving `s` unchanged.
This avoids interference with the existing single-witness flow
and lets the caller invoke it without committing to a goal mode.

## Implementation outline (for 9.1b)

```c
typedef struct {
  AtpState *s;
  RewriteSubst *witnesses;
  u32 max_witnesses;
  u32 found;
  u32 max_depth;
} NarrowAllCtx;

static void narrow_all_dfs(NarrowAllCtx *ctx,
                           Term lhs, Term rhs,
                           const RewriteSubst *acc_subst,
                           u32 depth) {
  if (ctx->found >= ctx->max_witnesses) return;
  if (kbo_eq(lhs, rhs)) {
    ctx->witnesses[ctx->found++] = *acc_subst;
    return;
  }
  if (depth >= ctx->max_depth) return;

  // Try every (position, rule) on lhs.  For each that unifies:
  //   - compose the new subst with acc_subst
  //   - recurse on (sigma_applied_lhs, sigma_applied_rhs, new_acc, depth+1)
  // Mirror for rhs.
  ...
}
```

The `cp_walk_positions` helper from `src/cp/_.c` provides the
position iteration; `thvm_unify` provides per-position
unification.  This is the same structure as 8.9b's
`thvm_atp_narrow_step` but recursive instead of greedy.

## WL surface (for 9.1c)

Extend `TATP[]` with an `AllWitnesses -> True` option that
calls a new LibraryLink entry `thvm_wl_atp_narrow_all`:

```mathematica
TATP[{f[a, e] == a, f[b, e] == a},
     f[Pattern[x, Blank[]], e] == a,
     Witness -> {Pattern[x, Blank[]]},
     AllWitnesses -> True]
(* -> <|"Status" -> "PROVED",
        "Witnesses" -> { <|x -> term_a|>,
                          <|x -> term_b|> }|> *)
```

When `AllWitnesses -> False` (default), TATP returns a single
`Witness` Association as before -- backwards-compatible.

## Test plan (for 9.1b/c/d)

- 9.1b: 4-6 unit cases on hand-built rule sets:
  - Zero witnesses: rule doesn't unify with any goal position
  - Exactly one witness: same as 8.9b's narrow_step
  - Multiple witnesses: two rules with distinct but compatible
    LHSs
  - Bound enforcement: max_witnesses caps the count
- 9.1c: WL test that verifies AllWitnesses -> True returns a
  list and the list is non-empty in a known-multi case.
- 9.1d: a `.pr` fixture with EXISTS + multiple witnesses; verify
  the bench harness picks it up (single-witness mode will just
  use first witness, but the parsing should be clean).

## Stop conditions

If 9.1b reveals that the DFS produces exponential branching on
small problems (rule combinations explode), document the
worst-case behavior and tighten the default bounds.

If 9.1c uncovers WL-side encoding issues (new LibraryLink entry,
witness-array NumericArray shape), defer the WL surface and
ship just the C-side helper as the deliverable.

If 9.1d shows that the existing bench harness's single-witness
mode disagrees with 9.1b's first-DFS result (DFS order vs greedy
position-walk order), document the difference.  This shouldn't
happen but is worth verifying.

## Out of scope

- Saturation interleaved with narrowing (saturate-and-narrow
  fixed-point): future work; 8.10's deferred research vector.
- Witness ranking by some priority (size, simplicity): v0
  returns DFS order.
- Unification modulo equational theories (AC, sort): outside
  9.1's scope.

## Verification

This is a documentation-only resolution.  No `make test` /
`make wl-test` impact; both remain green from the prior 9
opening.

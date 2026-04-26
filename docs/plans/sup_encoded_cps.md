# SUP-encoded CP enumeration: design memo (stage 8.1a)

> Sibling of `waldmeister_ic_atp.md` and `connectedness_design.md`.
> Decision document for stage 8.1's TAG_PRI + SUP machinery and
> migration target.

## Goal

Stage 8.1 moves critical-pair enumeration from C-side
(`src/cp/_.c::thvm_critical_pairs_range`) to native IC reduction.
The C version explicitly enumerates `(rule_a, rule_b, position)`
triples, runs unification at each, and accumulates surviving CPs
in a buffer.  The IC version reifies the cross product as a
nested SUP and lets DUP-SUP commutation do the enumeration; each
leaf invokes a `TAG_PRI` callback that performs the unification.

The motivation is twofold.  First, it puts CP generation in the
same evaluation model as the rest of the IC pipeline, which makes
SupGen-style search (stage 8.10) implementable -- 8.10 superposes
the *choice* of next CP, so CPs need to be reified as SUP entries
before they can be superposed.  Second, it lets optimal-sharing
collapse merge unification work across CPs that have shared
substructure (the SupGen "ADD-CARRY" effect, `~7,277x` on the
canonical benchmark per
[TinyHVM/resources/supgen_kernel_search.md](../../TinyHVM/resources/supgen_kernel_search.md)).

## TAG_PRI in HVM4 (reference implementation)

HVM4's clang backend has `PRI = 45` (see
`TinyHVM/HVM4/clang/hvm.c:98`) and a primitive table
(`prim/register.c`):

```c
typedef Term (*PrimFn)(Term *args);
typedef struct { PrimFn fun; u32 arity; } PrimDef;
static PrimDef PRIM_DEFS[BOOK_CAP];

fn u32 prim_register(const char *name, u32 len, u32 arity, PrimFn fun);
fn PrimFn prim_fun(u32 id);
fn u32    prim_arity(u32 id);
```

A `TAG_PRI` term carries an `id` (u32, fits in `EXT`) that indexes
the table.  The reducer collects `arity` arguments (via
APP-PRI partial application or via direct CTR construction) and
calls `fun(args)`, replacing the redex with the result.

Three things to port:

1. **The tag** -- `TAG_PRI = ?` in our `src/thvm.h` (see existing
   tag list, currently TAG_BRI=23, TAG_ANN=24; PRI = 25 fits).
2. **The registry** -- a small table mapping `id` to
   `(PrimFn, arity)`. Our `prim_register` sets entries; lookups
   are O(1).
3. **APP-PRI interaction** -- `app_pri.c`: an APP whose left
   child is a PRI accumulates the right child as an arg; once
   `arity` args have accumulated, fires the C function and
   replaces with the result.  Until then, holds onto the partial
   PRI as a CTR-shaped accumulator.

Tinyhvm-flavored implementation sketch:

```c
fn Term term_new_pri(u32 prim_id) {
  return term_new(0, TAG_PRI, prim_id, 0);
}

// On APP-PRI: fold one arg into the PRI's pending-args buffer.
// When buffer fills, fire PrimFn.
fn Term interact_app_pri(Term app_term) {
  // Peel APP, find PRI in left child.  Append the APP's right
  // child to PRI's argument cell.  When count == arity, call
  // PRIM_DEFS[id].fun and return its result; otherwise return
  // the partial PRI with one more arg.
}
```

## SUP-cross-product encoding for CP enumeration

The C-side `thvm_critical_pairs_range(lhs[], rhs[], n_rules,
i_lo, i_hi, j_lo, j_hi, out, cap)` iterates:

```
for i in [i_lo, i_hi):
  for j in [j_lo, j_hi):
    for each position p in lhs[i]:
      try thvm_unify(lhs[i] @ p, rename_fresh(lhs[j]))
      if successful:
        emit CP from rule i applied to rule j at p
```

The IC encoding reifies each axis as a labeled SUP:

```
ROLE_OUTER  = &L_outer { lhs[0], lhs[1], ..., lhs[n-1] }
ROLE_INNER  = &L_inner { lhs[0], lhs[1], ..., lhs[n-1] }
ROLE_POS    = &L_pos   { pos_0, pos_1, ..., pos_k }   -- per term
```

The cross product is built by APP'ing a primitive over the three:

```
APP(APP(APP(PRI cp_at_pos, ROLE_OUTER), ROLE_INNER), ROLE_POS)
```

APP-SUP commutation distributes the APP across the SUP children
left-to-right, producing one APP-PRI redex per `(i, j, p)` triple.
Each redex fires once `cp_at_pos` has all 3 args, calling the C
unification callback.  The callback returns either:

- `ERA` if unification fails -- collapses to nothing.
- A `CTR(cp_lab, [lhs', rhs'])` carrying the produced CP --
  flows up through the SUP structure and is collected by the
  outer DUP / collapse sequence.

The full set of CPs is the result of
`thvm_collapse(...)` over the whole expression.  Failed paths
short-circuit via ERA propagation; surviving CPs come out as a
flat sequence.

### Why labeled SUPs (not unordered) suffice

Standard SUPs are *labeled*: DUP-SUP commutes only when labels
*differ*; same-label is annihilation.  For the cross product, we
want commutation, so each axis needs a distinct label
(`L_outer`, `L_inner`, `L_pos`).

Unordered SUPs (stage 8.6) would let us write
`{lhs[0], lhs[1], ...}` without a label and have the runtime do
the right thing for any pairing -- but they are not yet in our
codebase or HVM4.  For 8.1, we allocate labels statically per
axis at CP-encoding time.  This works as long as the number of
axes is bounded (3 here) and known.  For deeper nesting (say,
N-way CP joins) we would need to allocate more label IDs;
this is a per-call concern not a structural blocker.

**Conclusion**: 8.1 does *not* require 8.6 (unordered SUPs).
The labeled approach is sufficient for the cross product.

## Migration target

Components that **stay in C** as `TAG_PRI` callbacks:

| Component | File | Why C |
|---|---|---|
| `thvm_unify` | `src/unify/_.c` | Robinson MGU has imperative bookkeeping (occurs check, stack of binding deferrals); IC encoding gives no clear win |
| `thvm_match` | `src/rewrite/_.c` | Same -- one-way matching is cheap C; not search-shaped |
| `thvm_subst_apply` | `src/rewrite/_.c` | Pure tree walk; no parallelism opportunity |
| `kbo_eq` | `src/kbo/_.c` | Hot path, called O(n) per CP filter; C is fine |
| Position enumeration | inline in `cp_at_pos` callback | Tiny helper; lives next to its only caller |

Components that **move to IC**:

| Component | Replacement |
|---|---|
| `thvm_critical_pairs_range` | SUP-cross-product expression + APP-PRI -> `cp_at_pos` callback |
| `atp_push_cps_traced`'s loop | DUP/collapse over the SUP expression; surviving CPs threaded with their `(parent_a, parent_b)` trace via tuple-CTR wrappers |

## 8.1 vs 8.10 ordering

Stage 8.10 (SupGen-style search inside saturation) wants to
superpose the *choice* of next CP and let priority-aware collapse
pick the cheapest first.  This requires CPs to be reified as
SUP entries with priority decorations -- which is exactly what
8.1 produces.

**Conclusion**: 8.1 unblocks 8.10.  Implement in order.

## Risks and open questions

1. **Label allocation across nested SUPs.**  If CP enumeration
   is itself called from inside another superposition (e.g. 8.10's
   "which CP to pick"), the labels must not collide.  Mitigation:
   a small per-call label-bump counter on `AtpState`, allocated
   from a high range outside the user-visible labels.
2. **Trace plumbing.**  The C-side `atp_push_cps_traced` knows
   the parent rules' trace indices.  In the IC encoding, each CP
   needs to be tagged with its parents.  We can extend the
   `cp_at_pos` callback to take `(rule_a_trace, rule_b_trace)`
   as additional args and fold them into the produced CP-CTR.
3. **Collapse cost.**  `thvm_collapse` walks the term graph; for
   a small rule set the graph is tiny and overhead is bounded,
   but for large `R` the SUP children fan-out can be wide.  Stage
   8.6 (unordered SUPs) would mitigate this; for 8.1 the working
   assumption is `n_rules <= 64` for the targeted workloads
   (group / monoid / small ring fragments).
4. **Performance baseline.**  The C version is hand-tuned and
   likely faster than the IC version for small problems.  8.1e's
   feature flag lets us fall back; we expect the IC version to
   pull ahead only when 8.10's superposed search is engaged.

## Verification (for stages 8.1b-e)

Each subsequent subtask lands an isolated piece:

- **8.1b**: tests/test_pri.c -- 4-6 cases.  Verify
  `term_new_pri`, APP-PRI argument accumulation, callback
  invocation, return value.  Cover arity 1 and arity 2.
- **8.1c**: tests/test_pri_unify.c (or extend test_pri.c) --
  hand-encode 2-3 (term, term) pairs with known unifier or
  known failure; invoke the unify callback via APP-PRI
  evaluation; verify substitution result equals the C-side
  `thvm_unify` result.
- **8.1d**: tests/test_sup_cps.c -- 3-5 cases.  Build a tiny
  SUP-cross-product expression for one rule pair at one or
  two overlap positions; collapse it; compare the resulting
  CP set against `thvm_critical_pairs_range` output for the
  same inputs.  Parity is the success metric.
- **8.1e**: bench harness re-run.  Feature flag in `AtpState`
  selects IC vs C path.  Re-run `make test` (must be green
  on both flag values) and `make bench-twee`.  Expected
  result: IC path within 2x of C path on `tests/data/atp/`
  fixtures; if not, document the gap and either tune or revert
  the flag default.

## Stop conditions

If 8.1b-c reveal that `TAG_PRI` is harder to integrate than
expected (e.g. the existing reducer's stack machine doesn't
support partial application cleanly), pause 8.1 and pick up
8.4 (multi-sort signatures) or 8.5 (LPO) -- both of which
unblock more problem categories without depending on 8.1.

If 8.1d shows that the IC encoding is structurally correct but
asymptotically slower than the C version, document this in
`docs/bench-atp.md` and treat 8.1 as research infrastructure
for 8.10 rather than a performance improvement on its own.

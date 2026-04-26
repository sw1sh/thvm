# Multi-sort signatures: design memo (stage 8.4a)

> Sibling of `waldmeister_ic_atp.md`, `kbo_ic_design.md`,
> `ic_rule_dispatch.md`.  Decision document for stage 8.4's
> port of single-sort assumptions to multi-sort.

## Goal

Stage 8.4 lifts the homogeneous-sort assumption that has held
through stages 1-7.  Today every CTR symbol implicitly takes
"any" arguments and produces "any" -- the parser explicitly
discards `SORTS` and `SIGNATURE`-supplied sort info
(`src/wald/_.c:233-237` and `:647-660`).  Real Waldmeister
problems (TPTP-UEQ `LCL`, `RNG`, `LAT` divisions in particular)
use multi-sort signatures, so deferring this further would
prevent us from running the real corpus.

## Where do sorts live?

Three placement choices:

### A. Parallel arrays on `WaldSpec`

```c
typedef struct {
  // ... existing fields ...
  char     sorts[WALD_MAX_SORTS][WALD_NAME_LEN];
  u32      n_sorts;
  // For each symbol i in 0..n_symbols, the sort metadata:
  u32      sym_arg_sorts[WALD_MAX_SYMBOLS][WALD_MAX_ARITY];
  u32      sym_result_sort[WALD_MAX_SYMBOLS];
  // For each var:
  u32      var_sort[WALD_MAX_VARS];
} WaldSpec;
```

Pro: minimal struct churn; sort indices are u32s consistent with
existing label/var-id conventions.
Con: spreads sort info across 3 arrays parallel to the existing
`symbols[]` / `vars[]`, awkward to keep in sync.

### B. Embed sort fields in `WaldSym` / `WaldVar`

```c
typedef struct {
  char name[WALD_NAME_LEN];
  u32  label;
  u32  arity;
  u32  prec_rank;
  u32  arg_sorts[WALD_MAX_ARITY];   // size = arity
  u32  result_sort;
} WaldSym;

typedef struct {
  char name[WALD_NAME_LEN];
  u32  var_id;
  u32  sort;
} WaldVar;
```

Pro: each metadata structure is self-contained; future code
that takes a `WaldSym*` already has the sort info.
Con: `WaldSym` grows by `WALD_MAX_ARITY * 4` bytes per slot
(~32 bytes if `WALD_MAX_ARITY = 8`); fixed worst-case
allocation.  But we already have `WaldSym[64]` slots so this is
only ~2 KB extra.

### C. Hybrid: name table on Spec, indices in metadata structs

Sort *names* in a flat table on `WaldSpec`; per-symbol /
per-variable sort *indices* stored in the metadata structs (B).
Avoids string duplication while keeping each WaldSym
self-contained.

**Decision: choice C.**  `sorts[][WALD_NAME_LEN]` lives on
`WaldSpec`; `WaldSym.arg_sorts[]`, `WaldSym.result_sort`, and
`WaldVar.sort` are u32 indices.  Mirrors how labels / var ids
already work (string in name field, u32 elsewhere).

## Where does sort-checking fire?

Two design options:

### Option I: precheck before `thvm_match` / `thvm_unify`

Add a helper `wald_sort_check(spec, term)` that walks a term
top-down and verifies each CTR's children's result sorts match
the parent's `arg_sorts[]`, and each FVR's id (resolved against
`vars[]`) matches the position's expected sort.  Called as a
gate before equations / CPs / orient-and-add inputs reach
saturation.

Pro: no changes to the matching / unification core; clean
separation; the saturation loop sees only well-sorted terms.
Con: pays for the walk on every input even though most
saturation-derived terms are well-sorted by construction
(induction on a well-sorted starting set).

### Option II: thread sort logic through `thvm_match` / `thvm_unify`

Modify the matchers to take the spec's sort tables and reject
on mismatched arg/result sorts during recursion.

Pro: single-pass; no separate walk.
Con: the matching code currently doesn't know about WaldSpec
(it works on raw Terms).  Threading the spec through is a
significant API change with implications for every caller.

**Decision: Option I.**  Precheck via `wald_sort_check`.  The
saturation engine assumes (post-precheck) that every term it
sees is well-sorted; the burden of maintaining that invariant
falls on the entry points (parser output, axioms, CPs).  Note
that CPs *can* be ill-sorted in principle if matching unifies
variables of different sorts -- but if we precheck both source
rules' LHSs to be well-sorted, the unifier inherits the sort
constraint via the variable bindings.  This is provable but
not enforced; if it bites us we can add an additional
precheck on each generated CP at minimal cost.

## KBO impact

Waldmeister's KBO supports per-sort variable weights and per-
sort symbol weights.  Sort-aware KBO is more discriminating
than the homogeneous version -- it can orient pairs that
single-sort KBO leaves incomparable.

For v0 (8.4 minimum), we accept the conservative answer: keep
single-sort KBO as-is.  The homogeneous KBO is sound (it never
orients a pair the sort-aware version disorients), just less
complete.  Adding sort awareness is a separate research item;
treat it as future work like 8.5's LPO.

## CP enumeration impact

Two adjustments are useful:

1. **Skip overlap pairs whose top symbols have incompatible
   result sorts**: if rule i's LHS produces sort `nat` and rule
   j's LHS produces sort `list`, no top-position overlap is
   possible.  This is a cheap precheck before unification.
2. **Inner-position overlap also needs the position's sort to
   match the inner-rule's result sort**: rule i overlaps rule j
   at position p iff `sort(lhs_i|_p) == sort(lhs_j)`.

For v0, we skip both optimizations.  The unifier will fail on
sort-mismatched pairs because the variables can't be coerced
across sorts (assuming we add a per-FVR sort tag in the
unifier, which we don't yet).  The cost is a slightly slower
unification step that fails late instead of early.

## Decision

**8.4b**: Implement choice C above on the data structures
(WaldSpec gets a sort name table; WaldSym / WaldVar embed sort
indices).  Update the parser to populate.

**8.4c**: Implement `wald_sort_check(spec, term)` per Option I
above.  Top-down walk, verifies each CTR vs its symbol's
arg_sorts; each FVR resolved against vars[] for sort.  Returns
0/1.

**8.4d**: Wire the precheck into `thvm_atp_add_equation` (gate
new equations) and `thvm_atp_set_goal` (gate the conjecture).
CPs are NOT prechecked since well-sortedness inherits from the
source rules' LHSs (documented assumption).

**8.4e**: Add a multi-sort `.pr` fixture, e.g. `nat_list.pr`:
`nat` and `list` sorts; `cons : nat list -> list`,
`zero : -> nat`, `succ : nat -> nat`.  Test that the parser
populates the sort metadata correctly.

## Unblocks

Closing 8.4 unblocks:

- **8.3d** (ICC TAG_BRI / TAG_ANN integration): per
  `docs/plans/ic_rule_dispatch.md`, BRI/ANN wrapping is
  meaningful only when there are sorts to type-check against.
- **8.4-followup** (sort-aware KBO; sort-aware CP precheck):
  performance optimizations gated on sort metadata being
  present.

## Verification (for stages 8.4b-e)

- 8.4b: parse the new `nat_list.pr` fixture; assert
  `n_sorts == 2`, sort names match, every WaldSym's sort
  metadata is populated, every WaldVar has the right sort.
- 8.4c: hand-construct well-sorted and ill-sorted terms;
  assert `wald_sort_check` returns the right answer.
- 8.4d: try to add a sort-mismatched equation via
  `thvm_atp_add_equation`; assert it's rejected.
- 8.4e: full saturation on `nat_list.pr` should still produce
  the expected outcomes (parity with single-sort behavior).

## Stop conditions

If 8.4b reveals that `WALD_MAX_ARITY` isn't defined or arity
representations conflict, defer to a sub-task that introduces
arity-storage as a pre-req.

If 8.4d's precheck rejects equations that the existing
homogeneous code accepted (because old fixtures use a single
sort but the new check requires explicit sort declarations),
back out the precheck for those fixtures by treating
`spec->n_sorts == 0` as "homogeneous mode, no checking" -- a
backwards-compat path.

If 8.4e demonstrates that running multi-sort saturation
exposes correctness bugs in the unifier (variables binding
across sorts), file a bug + add the per-FVR sort tag as a
follow-on stage.

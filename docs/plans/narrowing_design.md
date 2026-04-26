# Narrowing for existential goals: design memo (stage 8.9a)

> Sibling of `waldmeister_ic_atp.md`, `multi_sort.md`,
> `lpo_design.md`.  Decision document for stage 8.9's port of
> Waldmeister's existential-query support.

## Goal

Today the saturator answers **universal** equational queries:
"does `s = t` hold in the equational theory of R?"  We want to
also answer **existential** queries: "does there exist an `x`
such that `s(x) = t(x)`?"  In Waldmeister's terms, the
`NormaleZiele.c` ("normal goals") + `Zielverwaltung.c`
("goal management") modules implement this via narrowing.

Concrete examples:

- "Does there exist `x` such that `f(x, a) = b`?" -- find a
  witness `x` such that the equation reduces.
- "Is there a normal form for some open term `f(x, e)`?" --
  the answer is `x` (which is a variable, but a valid normal
  form for any binding).

The current `thvm_atp_set_goal(lhs, rhs)` rewrites both sides
under R and asks for structural equality.  For existential
goals we instead need to *narrow*: try unifying parts of the
goal with rule LHSs and propagate the substitution.

## Narrowing vs rewriting

| Aspect | Rewriting (current) | Narrowing (8.9) |
|---|---|---|
| Operation at a position | Match LHS against subterm; substitute RHS | Unify LHS with subterm; substitute RHS, accumulate σ |
| Variables in the goal | Treated as opaque atoms | Existentially quantified -- σ binds them |
| Termination on the goal | NF reached when no rule matches | NF reached when goal becomes `s = s` (after σ applied) |
| Result | Boolean (NF_R(lhs) =?= NF_R(rhs)) | Witness substitution σ on success |

The key difference: in rewriting, an FVR in the goal is just an
arbitrary fixed term we don't know.  In narrowing, an FVR in
the goal is a free variable we're solving for.

## API shape

### Existential variables: explicit list on the goal

The cleanest API: callers pass a list of FVR ids that are
"existential" (free for narrowing to bind).  Other FVRs in the
goal stay treated as opaque.

```c
fn u8 thvm_atp_set_goal_existential(
    AtpState *s,
    Term lhs, Term rhs,
    const u32 *witness_var_ids, u32 n_witness
);
```

This:
- Stores the witness ids in `s->witness_var_ids[]` (new field).
- Sets `s->goal_existential = 1` (new flag).
- Stores `lhs / rhs` in the existing `goal_lhs / goal_rhs`.

When `goal_existential == 0`, the saturator behaves exactly as
today (rewrite + structural equality).  When `1`, it narrows.

### Witness output

After saturation closes with `ATP_PROVED`, the bindings live in
`s->witness_subst[REWRITE_MAX_VAR]` (paralleling
`RewriteSubst.bindings[]`).  Retrieve via:

```c
fn Term thvm_atp_get_witness(const AtpState *s, u32 var_id);
```

Returns `0` (invalid Term) if the var isn't bound or if no
proof has closed yet.

## Saturation loop divergence

Today's loop (per `docs/plans/saturation_loop.md`):

1. `select_cp` -- pop next CP
2. `normalize` -- both sides under R
3. `trivialize` -- skip if equal
4. `orient` -- pick direction via KBO/LPO
5. `interreduce` -- simplify R against the new rule
6. `generate_cps` -- new x R + R x new
7. `goal_check` -- normalize goal lhs/rhs; PROVED if equal

Narrowing changes step 7 (and only step 7) when
`goal_existential == 1`:

7'. `narrow_check`:
    - Walk every non-variable position p of `goal_lhs`.
    - Try unifying `goal_lhs|_p` with each rule's LHS.
    - On success with substitution σ:
      - Apply σ to both `goal_lhs[p ← rhs_rule]` (rewriting at
        p) and `goal_rhs`.
      - If the two sides are now equal: PROVED, witness = σ
        restricted to the witness var ids.
      - Otherwise update goal_lhs (or goal_rhs) to the σ-applied
        form and continue narrowing.
    - Same for goal_rhs's positions.

This is **bounded backtracking**: at each step we try every
(position, rule, side) triple; on success we commit and continue
narrowing the new goal; on no-success across all triples,
saturation step continues normally (CP gen etc.) and we re-try
narrow_check next iteration with the bigger R.

## Termination

Narrowing in general is non-terminating (it's
semi-decidable).  Bounds:
- Standard `step_cap` already bounds saturation steps.
- Witness-search depth: bound the number of σ extensions per
  narrow_check call (a "narrow budget" parameter, default 8).

## Tradeoffs and open questions

- **Sound vs complete narrowing**: full narrowing requires
  *all* CPs (innermost-only narrowing isn't complete).  We get
  completeness from the saturation loop's CP closure -- once R
  is fully saturated, narrowing is complete on it.
- **Witness selection**: multiple σs may close the goal.  v0
  picks the first one found (in deterministic position-walk
  order); future work could enumerate all witnesses.
- **Variables in axioms vs goal**: axiom variables are renamed
  fresh during overlap (CP gen).  Goal variables are NOT
  renamed -- they're existential, so the same id across
  occurrences is a single quantified variable.

## Decision

Implement Strategy A (explicit witness-id list) above.

**8.9b**: `thvm_atp_narrow_step(s, lhs, rhs, witness_subst_out)`
helper that does one narrowing step.  Returns 1 on success +
populates `witness_subst_out`; 0 otherwise.

**8.9c**: integrate into `thvm_atp_goal_check` via flag
dispatch.  Add `s->goal_existential`,
`s->witness_var_ids[REWRITE_MAX_VAR]`, `s->n_witness_vars`,
`s->witness_subst` fields.

**8.9d**: extend `.pr` parser with an optional `EXISTS x, y, ...`
declaration before `CONCLUSION`.  Bench-harness fixture under
`tests/data/atp/`.

**8.9e**: WL surface `TATP[..., Witness -> {x_, y_}]` returning
`<|"Status" -> "PROVED", "Witness" -> <|x -> term, y -> term|>|>`.

## Verification

- 8.9b: hand-build a few narrowing cases:
  - Direct narrow: rule `f(a) = b`; goal `f(x_) = b` with witness
    `x` -> bind `x = a`.
  - Multi-step: rules `f(g(x)) = h(x)`, goal `h(x_) = h(a)` with
    witness -> `x = a` (after one narrow step on the rhs).
  - No witness: goal that doesn't unify with any rule -> 0.
- 8.9c: full saturation on a tiny existential proof; assert
  the witness binds correctly.
- 8.9d: round-trip a `.pr` file with `EXISTS` declaration.
- 8.9e: `TATP[{f[a] == b}, f[x_] == b, Witness -> {x}]`
  returns `<|"Witness" -> <|x -> a|>|>`.

## Stop conditions

If 8.9b reveals that the existing `thvm_unify` doesn't cleanly
support "narrow at a sub-position" (because the unifier expects
both inputs to be self-contained), add a small wrapper that
applies σ to the surrounding context after each narrow step.

If narrowing termination becomes a problem (each step adds new
CPs that re-enable more narrowing), document the depth-bound
parameter clearly and require callers to set it.

If 8.9c shows that the witness lifetime is harder than expected
(σ accumulates across many narrow steps; var ids may collide
with rule's renamed vars), consider a separate
`witness_subst_table` indexed only by the user-declared
`witness_var_ids` rather than the full `RewriteSubst`.

## Out of scope

- Multi-witness enumeration: just first-found in v0.
- Conditional narrowing (constructor-shaped guards): outside
  the unit-equational fragment.
- Higher-order narrowing: irrelevant (we're FOL).

# Saturation loop -- design sketch (stage 5)

> Stage 5 of [waldmeister_ic_atp.md](waldmeister_ic_atp.md).
> Composes the C-side primitives that landed in stages 1-4
> (`thvm_kbo`, `thvm_match`, `thvm_unify`, `thvm_critical_pairs`,
> `thvm_rewrite_normalize`, `thvm_collapse_ordered`) into the
> outer KB completion / proof-search loop.
>
> Companion to the algorithmic background in
> `waldmeister/documents/ShortDocumentation.txt` and Waldmeister's
> own `Hauptkomponenten.c` (*Hauptkomponenten* = "main components",
> function `HK_Vervollstaendigung` = "HK_completion").

## Algorithm

Standard unfailing Knuth-Bendix completion as a proof procedure.
Loop until the goal is shown, the queue is empty, or `step_cap`
fires:

1. **Select** the highest-priority CP `(s, t)` from the queue.
2. **Normalize** both sides under the current rule set `R` to
   `(s', t')`.
3. **Discard** if `s' == t'` syntactically; loop.
4. **Orient** with KBO: if `s' > t'` add rule `s' -> t'`; if
   `t' > s'` add `t' -> s'`; otherwise (incomparable) add the
   pair as a 2-way rule (a.k.a. unfailing fallback).
5. **Interreduce** `R` against the new rule: drop any rule whose
   LHS reduces under the new rule, requeue its RHS (treated as a
   fresh equation).  This is the *Waldmeister-Interreduktion*
   mechanism (`Interreduktion.c` "interreduction").
6. **Generate** new CPs from the new rule against every existing
   rule (`thvm_critical_pairs` over the singleton + R) and push
   them onto the queue.
7. **Goal-test**: normalize both sides of the goal under R; if
   they collide return `PROVED`; else continue.

## State

```c
typedef enum {
  ATP_RUNNING  = 0,
  ATP_PROVED   = 1,   // goal closed
  ATP_REFUTED  = 2,   // R is finite + ground convergent + goal sides differ
  ATP_TIMEOUT  = 3,   // step_cap exceeded
  ATP_QUEUE_EMPTY = 4,
} AtpStatus;

#define ATP_MAX_RULES 256
#define ATP_MAX_CPS   4096

typedef struct {
  // Rule set R: parallel arrays so `thvm_rewrite_normalize` /
  // `thvm_critical_pairs` can take them by pointer.  After
  // interreduction (step 5) the slot is reused; no holes.
  Term lhs[ATP_MAX_RULES];
  Term rhs[ATP_MAX_RULES];
  u32  n_rules;

  // CP queue.  Stored open-form (unwrapped); INC-wrapping happens
  // at enumeration time inside `atp_select_cp`.
  Term cp_lhs[ATP_MAX_CPS];
  Term cp_rhs[ATP_MAX_CPS];
  u32  n_cps;

  // Goal: a single equation s == t to prove.
  Term goal_lhs;
  Term goal_rhs;

  // Ordering.  For now, KBO with caller-supplied tables.  LPO
  // lands as 8.5 (deferred).
  const KboConfig *kbo;

  // Bounds.
  u32 step;
  u32 step_cap;
} AtpState;
```

The struct lives in `src/atp/_.c` (file path = function: `thvm_atp_run`,
plus `thvm_atp_init` / `thvm_atp_free` / `thvm_atp_step`).

## Step (one cycle of the loop)

`AtpStatus thvm_atp_step(AtpState *s)`:

1. If `s->step >= s->step_cap` return `ATP_TIMEOUT`.
2. If `s->n_cps == 0` return `ATP_QUEUE_EMPTY`.
3. **Select**: pick the cheapest CP via `thvm_collapse_ordered`
   over a SUP-tree of `INC^k`-wrapped candidates (5.3).  Remove
   it from the queue (compact the array).
4. **Normalize**: `lhs' = thvm_rewrite_normalize(cp.lhs, ...)`;
   same for rhs.  Uses recursive-descent rewriter from 5.4.
5. **Trivialize**: if `kbo_eq(lhs', rhs')`, increment `step` and
   return `ATP_RUNNING`.
6. **Orient**: `KboCmp c = thvm_kbo(lhs', rhs', s->kbo)`:
   - `KBO_GT`: add `lhs' -> rhs'`.
   - `KBO_LT`: add `rhs' -> lhs'`.
   - `KBO_EQ`: covered by step 5.
   - `KBO_UN`: add as a 2-way pair (insert both orientations as
     separate rules; stage 5.2 spec).
7. **Interreduce**: walk every existing rule; if its LHS reduces
   under the new rule, drop it and push the original equation
   back onto the CP queue.  Drop trivially-redundant rules.
8. **Generate**: `thvm_critical_pairs(R + {new}, R + {new}, ..., out, cap)`
   only across pairs *involving the new rule* (no duplicate work
   from the existing `R x R`); push surviving CPs onto the queue.
9. **Goal-test**: normalize both `goal_lhs` and `goal_rhs` under
   the (now-extended) `R`.  If `kbo_eq(g_lhs, g_rhs)` return
   `ATP_PROVED`.
10. Increment `step`, return `ATP_RUNNING`.

`thvm_atp_run` is just a `while (status == ATP_RUNNING) status =
thvm_atp_step(...)` driver.

## Fairness

KB completion as a *semi*-decision procedure requires that every
overlap be eventually selected.  Pure cheapest-first (size-based
priority) can starve large CPs forever if smaller ones keep being
generated.

Two mitigations:

- **`step_cap`** is a hard ceiling.  Forces termination; anything
  unselected is reported as `ATP_TIMEOUT`.  Sound: missing a
  selection only means we don't *prove* the goal; we never claim
  refutation incorrectly.
- **Round-robin escape valve**: every Nth step (configurable),
  ignore priority and pop the *oldest* CP regardless of cost.
  This is the *Waldmeister `--mix` heuristic*'s spiritual cousin,
  retrofitted to the `--add` priority queue we ship in 5.3.

Stage 8.8 lands the proper `--mix` heuristic (size + orientability)
and replaces the round-robin patch.

## Termination conditions

| Status | Meaning |
|---|---|
| `ATP_PROVED` | the goal sides reduced to syntactic equality under `R`; we have a valid proof. |
| `ATP_QUEUE_EMPTY` | `R` is saturated (no new CPs) and the goal didn't close.  In **proof mode** this means the equation is *not* derivable from the axioms (a refutation; valid only if `R` is ground convergent, which the unfailing variant guarantees on termination). |
| `ATP_TIMEOUT` | `step_cap` exhausted.  No conclusion either way. |
| `ATP_RUNNING` | not a real outcome; the loop continues. |

## Connection to the C-side primitives we already have

| primitive | role in saturation step |
|---|---|
| `thvm_match` (3.1) | inside `thvm_rewrite_normalize` (already used). |
| `thvm_subst_apply` (3.2) | inside `thvm_rewrite_normalize`. |
| `thvm_rewrite_step` / `_normalize` (3.3) | step 4 (normalize CP sides) + step 9 (goal-test).  Will be extended in 5.4 to recursive descent. |
| `thvm_unify` (4.1) | inside `thvm_critical_pairs`. |
| `thvm_rename_vars` (4.2) | inside `thvm_critical_pairs`. |
| `thvm_critical_pairs` (4.3) | step 8.  Wrapper today does the full `R x R` cross product; the saturation loop will call it with `(new_rule, R)` only to avoid re-deriving CPs that are already in the queue. |
| `thvm_kbo` (2) | step 6 (orient). |
| `thvm_collapse_ordered` (1.6) | step 3 (select cheapest CP). |
| `term_new_inc` (1.6) | wrap each CP candidate with `INC^k` for the priority sort. |

## Open questions / load-bearing assumptions

- **Memory growth.** `thvm_critical_pairs` and the CP queue
  allocate fresh CTR layers per CP.  No reclamation.  For a small
  rule set this is fine; for serious benchmarks (8.x) we need a
  per-saturation-step heap reset or GC-roots integration.
- **Variable id collisions across rules.**  `thvm_critical_pairs`
  uses `REWRITE_MAX_VAR/2` as the rename offset, which assumes
  all rules' variable ids fit in `[0, 32)`.  Saturation generates
  new rules whose RHS may reuse the original input vars; we need
  to canonicalize by re-numbering vars to a small dense range
  before inserting into `R`.  Helper: `thvm_canonicalize_vars(t)`.
- **CP queue growth bound.**  `ATP_MAX_CPS = 4096` is arbitrary;
  realistic UEQ benchmarks need 10^4 to 10^6.  We'll bump after
  the first sized benchmark in 7.4.
- **Subsumption in the queue.**  Stage 7.3.  Without it, the
  queue accumulates redundant CPs.  Mitigation in 5.3: size
  comparison + structural-eq dedup on insert.
- **Unfailing fallback (KBO_UN orientation).**  Adding a 2-way
  rule pair doubles the rule count for unorientable equations.
  Waldmeister's  approach: orient the rule both ways but record
  it as a *single* equation in the data structure, then at
  rewrite time try both directions and keep whichever is
  applicable.  We'll mirror that once 5.2 is otherwise stable.

## Demo target (5.5)

Given the standard group signature {e, i, f} and axioms

  f(x, e)        = x       (right identity)
  f(x, i(x))     = e       (right inverse)
  f(f(x, y), z)  = f(x, f(y, z))   (associativity)

prove `f(a, i(a)) = e` (the right inverse for a specific Skolem
constant `a`).  Step count target: under 30 saturation steps.

The proof is essentially direct: right-inverse instantiated with
`x = a` gives the goal in one rewrite.  But the saturation engine
will likely do extra work generating CPs from associativity
overlapping right-identity / right-inverse before checking.  The
demo confirms (a) the engine doesn't loop forever, (b) the goal
test fires when applicable, (c) `ATP_PROVED` is returned.

# Phase 0 + 1 Implementation Plan - Shared CC Substrate + Online CDCL(T)

The concrete, code-grounded build for the first (and highest-leverage)
step of the [unification roadmap](unification_roadmap.md), under the
**bottom-up** sequencing. Goal of these two phases: replace thvm's two
weakest SMT pieces - the WL reimplementation of congruence closure and
the offline blocking-clause DPLL(T) - with a **shared C substrate** that
every later theory (arithmetic, arrays) and the superposition engine
will build on.

## The seam today

Three facts from the current code set the shape of the work:

1. **`TSatEUF` is a WL reimplementation of congruence closure**
   (`wl/THVMLink/Kernel/ATP/SMT.wl:96-224`) - Downey-Sethi-Tarjan
   union-find over WL expressions, *separate* from the C engine. It is
   not incremental and produces no conflict explanations.
2. **The C saturation engine is incremental but batch-exposed.** The
   C API (`src/thvm.h:4691-4694`) is `thvm_atp_init(KboConfig*, cap)` /
   `thvm_atp_add_equation` / `thvm_atp_set_goal` / `thvm_atp_step`
   returning `AtpStatus` (`ATP_RUNNING=0`, `ATP_PROVED=1`,
   `ATP_SATURATED`, ...). But WL only reaches it through one batch
   `cEngineProof[...]` LibraryFunction - there is no assert/backtrack
   surface, and the engine carries CP-queue + ordering + redundancy
   machinery that a ground decision procedure does not want.
3. **DPLL(T) is offline.** `TSmtDecide` (`SMT.wl:327-366`) abstracts
   atoms, calls Wolfram `SatisfiabilityInstances`, theory-checks the
   model, and on conflict adds a *blocking clause* and re-queries -
   the textbook lazy loop, no theory propagation, no learning.

So the target substrate does not exist yet in either place. We build it
in C as a dedicated module, validate it *is* ground completion (the
roadmap keystone), then put an online CDCL(T) core in front of it.

## Phase 0 - `src/cc/`: a shared congruence-closure module

A dedicated C congruence-closure decision procedure over thvm's `Term`
heap, sharing the discrimination-tree index (`AtpRuleIndex`) with the
saturation engine. This is the design every production SMT solver uses
(a purpose-built CC, *not* the superposition engine) - but we keep the
abstract-congruence-closure equivalence as a **validated invariant**, so
the "CC is ground completion" keystone is operational, not just asserted.

### Module

New `src/cc/_.c` + declarations in `src/thvm.h`. Core state:

```c
typedef struct {
  // union-find over egraph nodes (each distinct subterm Term -> node id)
  u32     *parent, *rank;
  // per-class use list: compound terms whose args mention this class
  CcUseList *use;
  // signature table: (fn-label, arg-class-reps...) -> node id.
  // THIS IS the set of extension rules  f(c1..ck) = c  of abstract CC.
  CcSigHash sig;
  // pending merge queue (Downey-Sethi-Tarjan)
  CcMergeQ  pending;
  TermMap   node_of;        // Term -> node id (egraph hash-consing)
} CcState;

fn CcState* cc_init     (void);
fn u32      cc_add_term (CcState*, Term t);          // hash-cons + flatten
fn void     cc_assert_eq(CcState*, Term a, Term b);
fn void     cc_assert_ne(CcState*, Term a, Term b);
fn CcResult cc_check    (CcState*);                  // CC_SAT | CC_UNSAT
fn u32      cc_rep      (CcState*, Term t);           // class representative
fn void     cc_classes  (CcState*, /*out*/ ...);      // SAT model partition
```

`cc_add_term` flattens each compound `f(a,b)` by interning it in the
signature table - which is exactly the extension-constant step of
abstract congruence closure. The signature table *is* the convergent
ground rewrite system; `cc_rep` reads off the normal form.

### Validation (the keystone, made operational)

`tests/test_cc.c`, two differentials over random ground QF_UF instances:

* **vs the WL `TSatEUF` partition** - same SAT/UNSAT, same classes.
  Guards the port.
* **vs ground completion through `thvm_atp_*`** - flatten the same
  equations to extension-constant form, run the *saturation engine* on
  them to saturation, and check its convergent rules induce the *same*
  class partition as `cc_check`. This is the abstract-CC invariant; it
  is what licenses Phase 4 (superposition modulo this substrate) and
  follows thvm's existing `THVM_ATPFT_NORM_VERIFY` differential
  discipline.

### WL rewiring

Add a `ccDecide` LibraryFunction binding (alongside `cEngineProof` in
`ATP.wl`). Replace the *body* of `TSatEUF` (`SMT.wl:193-224`) with a
thin marshal to `ccDecide`, keeping the public surface (inputs,
`"Status"`/`"Classes"`/`"Witness"`) byte-identical so `atp_smt.wlt`
stays green. The WL CC (`ccInit`/`ccUnion`/`congruencePropagate`, ~130
lines) is deleted once the native path is default.

### Landing

1. `src/cc/` + `tests/test_cc.c`, default-off; WL `TSatEUF` unchanged.
2. Flip `TSatEUF` to `ccDecide` behind `THVM_SMT_CC_NATIVE`; burn in
   against the WL path on `atp_smt.wlt` + the differential.
3. Make native the default; delete the WL CC body.

## Phase 1 - online CDCL(T)

Two sub-pieces: make the CC incremental + explaining, then put a real
CDCL core in front of it.

### 1a. Incremental, proof-producing CC

Upgrade `CcState` to the Nieuwenhuis-Oliveras *proof-producing*
congruence closure:

* **Explain tree.** Each union records an edge `(a, b, reason)` where
  `reason` is the input literal or a congruence-pair justification.
  `cc_explain(a, b) -> CcLitSet` walks both nodes to their nearest
  common ancestor in the proof forest and unions the edge reasons,
  yielding a (near-minimal) conflict explanation.
* **Backtrack.** A merge trail + `cc_push()` / `cc_pop(n)`; union-find
  undo via the standard "record the redirected root" trail (no path
  compression across a checkpoint, or recompute on pop). Lets the
  theory solver track the SAT decision level.
* `cc_assert_eq` returns the list of atoms it *propagates* (entailed
  equalities now forced), feeding theory propagation below.

New `tests/test_cc_explain.c`: every conflict explanation, re-asserted
alone, must reproduce the conflict (soundness probe in the
`NORM_VERIFY` spirit) and should be subset-minimal under one pass of
literal-drop.

### 1b. CDCL(T) core - `src/sat/`

A watched-literal CDCL solver in C with a DPLL(T) glue layer
(Nieuwenhuis-Oliveras-Tinelli framework):

```c
fn SatState* sat_init   (void);
fn void      sat_add_clause(SatState*, Lit*, u32 n);
fn SatResult sat_solve  (SatState*, CcState* theory);  // SAT | UNSAT
```

* Equality atoms abstract to Boolean vars (the `collectAtoms` step,
  now in C).
* **Theory propagation:** after each decision, `CcState` propagates
  entailed atoms back as SAT unit implications (with a lazy explanation
  generated on demand for conflict analysis).
* **Theory conflict:** when an assignment forces a `cc_check` UNSAT,
  `cc_explain` produces the conflict clause; CDCL learns it and
  backjumps, popping the `CcState` trail in lockstep.

This replaces the `SatisfiabilityInstances` + blocking loop entirely.
WL `TSmtDecide` (`SMT.wl:327-366`) becomes a marshal to a new
`smtDecide` LibraryFunction; surface (`"Status"`/`"Model"`) unchanged.

*Interim fallback (de-risking):* before the full CDCL core lands, an
eager-theory-propagation half-step - push congruence-entailed atoms
into the existing WL `SatisfiabilityInstances` call as extra clauses -
recovers much of the scaling without the new C solver, and validates
the propagation interface. Keep it under an env flag, not as the
endpoint.

### Validation

* **Correctness:** same SAT/UNSAT as current `TSmtDecide` across
  `atp_smt.wlt`.
* **Scaling:** diamond / pigeonhole EUF families where blocking-clause
  DPLL(T) is exponential and CDCL(T) is not; a QF_UF SMT-LIB smoke set.
  Report wall + conflict counts vs the offline path.
* **Soundness:** the explain-soundness probe (1a) plus a model-replay
  check (every reported SAT model satisfies the original formula under
  `cc_check`).

### Landing

4. `src/cc/` incremental+explain upgrade + `tests/test_cc_explain.c`.
5. `src/sat/` CDCL + DPLL(T) glue + `smtDecide` binding, behind
   `THVM_SMT_CDCL`.
6. Flip `TSmtDecide` default to native; keep the offline path reachable
   under env for differential regression.

## TFindFiniteModels - the finite-model application + speed benchmark

A native finite model finder reimplementing the WFR `FindFiniteModels`
over the substrate. It is both a genuinely useful surface (the
constructive dual of the disproof path) and the **speed benchmark** for
the whole substrate effort - `FindFiniteModels` / the kernel's Mace-style
backend is slow, so beating it is a concrete, falsifiable target distinct
from the `SatisfiabilityInstances` comparison.

### Why it rides the substrate cleanly

Finite model finding over an equational signature *is* a query the
substrate answers natively. For a fixed domain size `n`:

* Introduce domain elements `d_0..d_{n-1}`, asserted pairwise distinct.
* Ground each (pattern-variable) axiom over the domain - instantiate its
  universals over `d_0..d_{n-1}` - giving ground equalities that must hold.
* For each operation cell `f(d_i, ...)`, assert **totality**: it lands in
  *some* domain element (`Or_v f(d_i,...) = d_v`).
* Hand the conjunction to the same congruence-closure + DPLL(T) substrate;
  read the model off the quotient as Cayley tables.

The structural win - and the reason "shouldn't be hard to beat":
**functionality is free in EUF.** A pure-SAT MACE/Paradox encoding spends
`O(n^(k+1))` clauses asserting each `f` cell has *at most one* value; over
congruence closure a cell `f(d_i,d_j)` is one term denoting one class, so
functionality costs **zero** clauses - only the `n`-way totality
disjunction remains. Nested ground terms (`g(g(d_0))`) need no extra cells;
congruence resolves them to base cells automatically.

### Bootstrap now, accelerate later

Build it today over the *existing* substrate (`TSmtDecide` + `TSatEUF` +
the disproof path's `atpClassIndex`/`atpCayleyTable` model extractor). That
gives a parity test vs `FindFiniteModels` and a benchmark harness *before*
any C lands. When Phase 1 replaces `TSmtDecide`'s offline blocking-clause
loop with the C CDCL(T) core, `TFindFiniteModels` speeds up with **no
surface change** - which is exactly the measurement that proves the
substrate fast.

### Validation protocol

* **Parity:** for representative theories (involution `g(g(x))=x`, small
  groups, Boolean algebra) the set of models at size `n` matches
  `FindFiniteModels` (up to isomorphism).
* **Speed:** wall-clock vs `FindFiniteModels` on the same theories /
  sizes - the headline number. Report both the current-substrate timing
  (the floor to beat with C) and, post-Phase-1, the C-substrate timing.

### The symmetry-breaking lever

Returning *one* model needs no symmetry breaking - one SAT call suffices.
Enumerating *distinct* models, or proving *no* model of size `n` exists
(UNSAT), is where the `n!` isomorphic-copy blowup bites, and a Least Number
Heuristic / canonical-cell-ordering constraint set is the standard fix.
v1 ships find-one + naive blocking-clause enumeration (isomorphic copies
allowed, logged); LNH symmetry breaking is the first follow-up and the
main UNSAT-side performance lever - it pairs naturally with the C CDCL(T)
core, where the canonical constraints become learned clauses.

## What this unlocks

After Phase 0+1 thvm has: a shared, native, incremental,
proof-producing equality substrate, driven by an online CDCL(T) core.
That is the exact spine Phase 2 hangs a Simplex arithmetic solver off
(Nelson-Oppen over the shared `CcState` class reps), Phase 3 feeds
E-matching instances into, and Phase 5 (AVATAR) uses as its splitting
oracle. Everything downstream is additive theories on this spine -
which is why it is the first thing to build.

## Open design decisions to settle before coding

* **Egraph term encoding:** curry to binary `app` nodes vs keep n-ary
  signatures. N-ary matches the saturation engine's `Term` shape and
  the existing `congruentQ` arity check; currying simplifies the
  signature table. Lean n-ary for substrate-sharing.
* **CDCL: build vs embed.** A minimal in-house watched-literal CDCL
  (~1-2k LOC) keeps the theory-propagation interface white-box (the
  whole point vs Vampire's black-box Z3). Revisit only if it becomes a
  bottleneck.
* **Where `collectAtoms`/abstraction lives** - moving it to C couples
  the SAT core to thvm term encoding; keeping a thin WL pre-pass keeps
  the binding simple. Start WL-side, push down if profiling says so.

## See also

- [unification_roadmap.md](unification_roadmap.md) - the full phase arc
  this is the first step of.
- [survey.md](survey.md) - the starting-point assessment.
- `wl/THVMLink/Kernel/ATP/SMT.wl` - the WL CC + offline DPLL(T) being
  replaced.

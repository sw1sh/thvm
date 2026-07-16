# thvm CASC strategy: how FindProof proves competition problems

This describes the strategy the `FindProof` CLI (`tools/thvm_atp_cli.c`) runs
for the CASC (CADE ATP System Competition) UEQ (Unit EQuality) division and
the day-1 FOF (First-Order Form) lane: the problem pipeline, the concurrent
slice portfolio, the underlying saturation engine, and the proof story.

Current standing (as of 2026-07-16; 240 s wall, pod hardware,
`THVM_ATP_SCHED_MAX_PAR` = slice count; TSVs in
`~/.cache/thvm/casc/pod_results/`):

| corpus | thvm | official winner (120 s StarExec) | Vampire 5.0.1 from source, same pod + wall |
|---|---|---|---|
| CASC-30 UEQ | 281/300 | Vampire 263 | 269 |
| CASC-29 UEQ | 282/300 | Twee 2.4.1 260 | 271 |
| CASC-J12 UEQ | 279/300 | Vampire 4.9 265 | 273 |

FOF: 112/500 of a stratified FEQ+FNE sample at 240 s (84/400 FEQ +
28/100 FNE) through the hybrid clausification pipeline of section 3.1 -- a
first measurement, not yet a competitive FOF entry.

## 1. The shape of the problem

A UEQ problem is a set of equations (the axioms) plus one negated
conjecture: a single inequality `s != t` whose two sides are ground after
Skolemization. Proving the theorem means deriving `s = t` from the axioms by
equational reasoning, i.e. refuting `s != t`. The classic engine for this is
**unfailing Knuth-Bendix completion**: orient equations into rewrite rules
under a reduction ordering, generate critical pairs (overlaps) between rules,
normalize them, and keep going until the goal's two sides reduce to a common
normal form. thvm's engine is a byte-faithful port of **Waldmeister** (the
historically dominant UEQ prover) extended with mechanisms borrowed from
**Twee** and **Vampire**.

## 2. The pipeline: from `.p` to a proof

A single `FindProof problem.p` invocation does the following, in order.

### 2.1 Parse + include splicing
`tp_preprocess` strips comments and splices one level of `include('Axioms/...')`
directives, resolving them against the problem directory, `$TPTP`, and the
current directory. A missing/unresolvable include is a hard
`SZS status Error`, not a silent GaveUp.

### 2.2 Problem transforms (all in the CLI, before the engine sees anything)
These run on the parsed clause set, each gated so it is a no-op when
inapplicable:

- **SInE axiom selection** (`tp_sine_select`): for oversized knowledge bases
  (CSR family, 8k+ clauses) goal-directed relevance pruning cuts the problem
  to the reachable slice before the capacity checks. The trigger discipline
  is Vampire-shaped: a symbol triggers an axiom when its occurrence count is
  within tolerance of the axiom's least-general symbol, with a generality
  threshold and a rare-symbol cap (`THVM_ATP_SINE_TOL` /
  `THVM_ATP_SINE_GT` / `THVM_ATP_SINE_RC` override; `THVM_ATP_SINE=0`
  disables).
- **FOF / non-unit CNF front end** (`--fof`, default on): clausify and
  encode into unit equations -- section 3.1.
- **Wide-functor currying** (`tp_curry_wide`, `--curry`): an n-ary
  functor with arity > 8 (e.g. TOP052's `tuple/16`) is decomposed into nested
  binary `ap2` applications so the arity-8 flatterm reader can load it. Sound
  bijective re-encoding.
- **Definition unfolding** (`tp_def_unfold`, `--def-unfold`): a pure function
  definition `f(X..) = t` is replaced everywhere and the axiom dropped
  (Vampire's `definition_unfolding`). `=2` means give up immediately when no
  definition exists (the exit-fast slice discipline).
- **Existential-goal equals encoding** (`tp_equals_goal`, `--equals-goal`): a
  Twee `addNarrowing`-style encoding of a non-ground goal via eq/true/false,
  for the existential family (COL009-1).
- **Critical-goal queue narrowing** (`--narrow-queue`): best-first
  goal-descendant narrowing lane for existential goals (the Waldmeister
  `PM_Existenzziele` / GKC `query_preference=1` shape); complementary to the
  equals encoding on the same family.
- **Encoded-Horn release** (`--horn`, `--horn-cd`, `--horn-back`): the
  condensed-detachment (LCL) problems are encoded Horn clauses; these run
  forward chaining, goal-directed backward CD with iterative-deepening DFS,
  and generalization-subsumption under Otter-style max-weight discipline.
- **Goal transformation** (`--tgt`): name compound subterms of the negated
  conjecture with fresh minimal symbols, bottom-up (Vampire
  `twee_goal_transformation`; `1` full, `2` ground-only, `3` also names
  axiom subterms, Vampire's `gtg=all`).
- **Canonical input presentation** (`tp_canon_input`, `--canon-input`, default
  ON): orient every axiom heavy-side-left (by KBO weight, ties by canonical
  clause text under first-occurrence variable renaming) and stable-sort the
  clauses. This makes the emitted `.pr` **presentation-invariant** -- the same
  theory presented three different ways (TPTP release variations) emits
  byte-identical input. It is score-neutral in aggregate but flips individual
  problems both ways, which is why the schedule carries `--canon-input=0`
  recovery slices.

Engine-side (not clause-set) transforms ride along as flags: `--ifeq-guard`
(guard-aware CP selection on ifeq-encoded problems, with `--ifeq-penalty`),
`--ifeq-strip` (formation-time resolved-guard stripping for the
LazyNormalize raw class), and the opt-in `--goal-narrow` ground
congruence-closure + goal-cone e-matching endgame (`src/atp/goal_narrow.c`).

### 2.3 `.pr` emission
`tp_emit_pr` serializes the transformed problem into the Waldmeister `.pr`
format (weights, precedence, ORDERING section, equations, goal). The engine's
`.pr` ingestion path is byte-identical to the WL bench, so a CLI run with a
given knob set reproduces a bench run exactly.

## 3. The portfolio: concurrent slices

`FindProof --schedule` runs a **portfolio** of strategy slices concurrently
(`tools/thvm_atp_cli.c`, the `SCHEDULE[]` table -- 29 slices as of
2026-07-16). Each slice is a full `FindProof` invocation with a distinct flag
set, forked into its own process group, each given the full wall clock; the
**first slice to print `SZS status Unsatisfiable` wins** and the rest are
killed. Concurrency width is `THVM_ATP_SCHED_MAX_PAR` (default 5; the
StarExec run script pins it to the node's core count so the whole portfolio
races in one wave -- under a hard wall, later waves never run). Exit-fast
slices (the `=2` gates below) free their pool slot immediately on
inapplicable problems, backfilling the rest.

The slices, and what each is for:

| slice | flags (essence) | targets |
|---|---|---|
| `wm` | the Waldmeister preset | the strongest general UEQ solver; most problems |
| `equals` | `--equals-goal=2` | existential goals (COL009-1); exits fast otherwise |
| `narrowq` | `--narrow-queue=2` | existential goals via goal-descendant narrowing (COL011-1); complementary to `equals` |
| `horn-cd` / `horn-back` | `--horn-cd=2` / `--horn-back=2` | condensed detachment, forward and backward (encoded-Horn LCL family) |
| `fof-*` (7 slices) | `--fof-slice=2` + one front-end lever | the FOF portfolio pack -- section 3.1 |
| `full-ac` | `--full-ac=2` | ring/lattice AC-completion (RNG021 family) -- section 5; first wave, exits fast on non-AC problems |
| `unfold` | `--def-unfold=2` + the flatten dressing | pure function definitions: unfolded + flattened, COL002-5's 6-superposition proof surfaces in 0.02 s under any presentation (verified 2026-07-16) |
| `unfold-tgt` | `--def-unfold=2 --tgt=1 --preset=none --horn=0` | definition-heavy conjectures where neither transform alone reaches the proof: KLE122-10 in 6.7 s (verified 2026-07-16; Vampire's official time 44.4 s) |
| `flatten` | `--flatten-goal --lazy-ground-join --backward-subsume --backward-demod --auto-max-weight=6 --max-weight-slope=1` | deep-goal UEQ completion stalls on (COL060, GRP167) |
| `flatten-bare` | `--flatten-goal --horn=0` | wall-edge flatten proofs the dressed extras push over the limit (ALG212-10) |
| `twee` | `--simpler-join --flatten-goal --lazy-ground-join --backward-subsume --backward-demod --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5` | the Twee-native completion regime (lattice/group residuals: LAT149, GRP177/706/753/766) |
| `mnf-gj-amw` | `--mnf --ground-join --connectedness --auto-max-weight=6 --max-weight-slope=1` | goal-directed MNF front + ground join |
| `encompass` | `--encompass-demod --max-steps=5000000` | AC/comm-heavy families that drown in variants (MVA003, LAT163) |
| `bare` | `--preset=none --horn=0` | WM-preset-divergent problems (BOO014 ifeq-encoded) |
| `ifeq` | `--ifeq-guard=2 --flatten-goal --goal-subterm --preset=none --horn=0` | guard-aware CP selection on the ifeq-encoded '-10' family; exits fast otherwise |
| `tgt` | `--tgt=1 --preset=none --horn=0` | goal-subterm naming (SYN615-10) |
| `twee-raw`, `bare-raw`, `flatten-bare-raw` | `--canon-input=0` variants | recover problems whose ORIGINAL presentation the canonical form loses (LAT149-1, BOO015-10) |
| `wm-gj`, `lazy-gj`, `bare-gj` | ground-join backups | launch only when an early slice exits |

Every UEQ-tuned slice also carries `--fof-sat=0` (see the pre-pass dedup note
in section 3.1). The portfolio is the design: no single strategy proves the
whole corpus, so diverse selection/ordering/redundancy regimes race, and each
problem is won by whichever regime fits it.

### 3.1 The FOF pack (`--fof-slice=2` slices)

On `fof(` (or non-unit CNF) input the front end (`--fof`, default on)
clausifies (`src/fol/cnf.c`: NNF, skolemization, distribution up to
`--fof-distrib`, Tseitin past it) and encodes the clause set into unit
equations: Horn clauses via the '-10' `ifeq` scheme, non-Horn clauses via a
sound-but-incomplete or/eq reification (`--fof-or`; `0` restores the
Horn-only give-up, `2` eq-reifies predicate atoms too). Before the equational
lanes run, a bounded clausal-saturation pre-pass (`fof_clausal_prepass`,
`src/fol/sat.c`) races resolution/superposition on the clausal form for up to
60% of the remaining wall (step budget `--fof-sat`).

Every UEQ slice used to run that identical front end and differ only in
engine knobs the pre-pass never reads -- measured on a 56-problem stratified
FEQ+FNE sample at 60 s, the dominant failure class (26/56 Timeout) was one
strategy racing itself. The FOF pack varies the levers that DO matter for the
front end; each slice exits immediately on non-`fof(` input (raw-text probe
`tp_input_is_fof`, the `equals`/`horn-cd` `=2` discipline), so on the UEQ
corpus the pack costs one fork + read per slice.

| slice | flags | front-end lever |
|---|---|---|
| `fof-fifo` | `--fol-age-ratio=1` | pre-pass given-clause selection: pure FIFO fairness |
| `fof-best` | `--fol-age-ratio=0` | pure best-first (no age interleave; default is a 5-to-1 interleave) |
| `fof-flat` | `--fol-goal-tier=0` | drop the conjecture-ancestry tier: flat weight order |
| `fof-deep` | `--fof-sat=1000000` | wall-bounded pre-pass (the default 20k-step cap, not the wall, starves SInE'd cones) |
| `fof-nopre` | `--fof-sat=0` | no pre-pass: the Horn/or equational lanes get the whole wall |
| `fof-or2` | `--fof-or=2` | eq-reified or-lane encoding (predicate atoms as `eq(p,true)`) |
| `fof-full` | `--fof-sine=0` | pre-pass on the FULL clausified set where the default SInE-cones it |

Three mechanisms ride along:

- **Pre-pass SInE** (`--fof-sine`, default ON above 512 clausified clauses):
  `fof_sine_cone` replays `tp_sine_pass`'s trigger discipline (generality
  threshold `gt=1`, rare-cap `rc=64`, tolerance ladder 1.5/1.2/1.0 at full
  trigger depth) at the `FolClause` level, so the bounded saturation works the
  goal-relevant cone instead of an 11k-clause knowledge base. Only the
  pre-pass input shrinks -- the equational lanes still see every clause, and a
  subset refutation is still a refutation. When no tolerance fits the
  threshold the LOOSEST cone stands (there is no capacity cap to fit, and
  over-tightening provably loses refutations). A cone that SATURATES without
  refuting is a proven miss for that subset, so the pre-pass retries once on
  the full clause set under the remaining wall guard.
- **Oversized-clause drop** (`fof_ser_emit`/`tp_fof_push` soft mode): an
  axiom-side clause whose ifeq-encoded serialization blows the 2048-char
  `.pr` side bound is DROPPED instead of killing the run -- the Mizar SET007
  families (ALG215+2, CAT034+2) carry a handful of giant definition clauses
  and every slice died on the first one. Dropping only shrinks the set;
  the goal-side pushes keep the hard give-up.
- **Pre-pass dedup**: every UEQ-tuned slice carries `--fof-sat=0`. Their
  pre-pass runs duplicated the `wm` slice's byte-for-byte (same clausified
  input, same selection) and burned up to 60% of the slice wall before the
  engine ever differed; on FOF input the pre-pass now runs only in the lanes
  that vary it (`wm` keeps the default discipline).

## 4. The saturation engine (what a slice actually does)

Each slice runs the shared completion engine (`src/atp/_.c`, ~31k lines,
single translation unit; flatterm cells `AtpFtCell`, not heap terms). The core
loop is unfailing KB completion:

1. **Orient** each equation into a rule under the reduction ordering (KBO by
   default, LPO available via `--ord=lpo`). Unorientable equations become
   two-way constrained rules.
2. **Select** a critical pair from the passive queue. Selection is the heart of
   the strategy: the WM preset uses Waldmeister's MixWeight + formation-age
   interleave; the `twee` slice uses Twee's CP score (`--cp-weight=8`), pure
   best-first (`--w2-modulo=0`), and Twee's passive re-scoring discipline
   (`--cp-set-ir-budget`).
3. **Normalize** the selected pair against the current rule set (the flatterm
   normalizer, discrimination-tree indexed).
4. **Redundancy**: drop the pair if it is joinable / subsumed. Levers here:
   ground-joinability (`--lazy-ground-join`), backward subsumption/demodulation,
   the Twee `simplerThan`-gated join (`--simpler-join`), encompassment
   demodulation (`--encompass-demod`).
5. **Add** surviving pairs as new rules, interreduce, and check whether the
   goal's two sides now have a common normal form. If so: PROVED.

Key faithfulness details that took the most work:

- **Lazy rule store**: rule slots hold TAG_NUM sentinels at birth, mirroring
  authoritative flatterm sides; materialize-at-birth policy keeps the default
  trajectory byte-identical to Waldmeister.
- **Single-walk CP formation**: native critical-pair formation matching WM's
  trajectory (default on).
- **N-way rank cache**: the CP selection comparator is memoized byte-identically.
- The default `wm` path is **byte-faithful to real Waldmeister** -- verified by
  insert-trace stream comparison against reference runs (143/148 notable-corpus
  rows byte-perfect; the residue is WM-side timeouts). All extensions are
  opt-in flags or separate slices, never changes to that path.

## 5. Borrowed mechanisms (the extensions over Waldmeister)

- **Twee's join discipline** (`--simpler-join`, `--cp-set-interreduce`,
  `--orphan-murder=0`): the `simplerThan`-gated join culls CPs plain
  joinability keeps; passive re-scoring on Twee's task discipline (freq 10s +
  5% budget, `--cp-set-ir-budget=5`) instead of an every-16-rules full sweep
  (3x the throughput); removing Waldmeister's Waisenmord (orphan murder) so
  parent-collapsed CPs survive to be re-scored the way Twee does. These flip
  the lattice/group residuals (LAT149, GRP177/706/753/766).
- **AC-completion** (`--full-ac`): the engine has a full AC subsystem
  (AC-matching, Stickel AC-unification, AC-flatten/canon in `src/atp/ac.c`)
  that used to be compiled in but inert. `--full-ac` auto-detects AC operators
  (from their commutativity+associativity axioms) and runs AC-modulo
  completion; `=2` gives up instantly when no AC operator is detected. This
  flips the ring family (RNG021-6 verified 1.2 s on 2026-07-16, plus
  021-7/025-5/027-7/029-5/029-6) that plain completion drowns on -- the same
  problems Vampire solves in 0.3 s, but by a different route (superposition,
  not AC).
- **Vampire's definition unfolding + goal naming** (`--def-unfold`, `--tgt`,
  and their composition -- the `unfold`/`unfold-tgt`/`tgt` slices above).
- **Existential-goal lanes**: the Twee equals encoding (`--equals-goal`) and
  the goal-descendant narrowing queue (`--narrow-queue`).
- **Vampire's LPO ordering** (`--ord=lpo`): LPO orients permutative laws KBO
  cannot. Wired but not in the schedule -- realizing Vampire's LPO gains needs
  its per-problem precedence+selection tuning. `--sym-prec` exposes the
  frequency / reverse-frequency symbol precedences to pair with it.
- **Encoded-Horn condensed detachment**: the LCL family is not equational
  completion at all; it is Horn-clause CD, run with forward/backward chaining
  and Otter-style max-weight + generalization-subsumption discipline.
- **Ground-CC endgame** (`--goal-narrow`, opt-in): ground congruence closure
  over R's ground slice + goal-cone e-matching (`src/atp/goal_narrow.c`),
  budgeted by the `--gn-*` caps.

## 6. The proof story

Competition output is quiet by default: in `--schedule` mode stdout carries
exactly one SZS status line (plus the proof block), and the engine's progress
chatter is suppressed; `--verbose` (or `THVM_ATP_VERBOSE=1`) restores
everything for development.

A winning slice can emit a full proof (`--proof`): `cli_proof_hook` sources the
goal from GC-rooted engine state, `cli_resolve_step_trace` replay-verifies each
citation in the trace domain, and treatment-compressed records are expanded
into single-rewrite steps at emission so every emitted step is one verifiable
rewrite. Provenance is explicit end-to-end:

- **UEQ**: `cnf(..., inference(orient|step|cp, ...))` chains from file-cited
  axioms to `$false` in an `SZS output start CNFRefutation` block.
- **Horn releases**: released units carry `inference(hyper_resolution, ...)`
  records citing their premise cone.
- **FOF**: the conversion chain is emitted per record --
  `nnf_transformation -> skolemisation -> cnf_transformation` -- followed by
  `resolution` / `superposition` / `equality_resolution` steps from the
  clausal pre-pass, closing at `$false` (verified on SEU140+2, 1.4 s,
  2026-07-16).

Verification is two-layer, both verified 2026-07-16 on a fresh LAT171-1
portfolio proof (0.3 s to prove):

- `tools/casc/proof_dag_check.py proof.p` -- a fast closed-DAG validator:
  every cited parent exists, no cycles, the derivation closes at `$false`
  ("135 records, 15 leaves, CLOSED").
- `FindProof --check --szs proof.p` -- the independent checker
  (`tools/proof_check.c`, the ProoVer CheckProof entry): replays equational
  routes, superposition replay for critical-pair records,
  unfailing-completion sessions for the residue, per-step OOM containment
  (`% SZS status VerifiedGood` in 0.01 s on the sample).

## 7. The open gap (as of 2026-07-16)

19 CASC-30 problems remain unsolved at the 2026-07-14 240 s sweep:
COL002-5, the deep group cluster (GRP196-1, GRP661..664-12, GRP678-1,
GRP690/692/694/695-1), KLE122-10, LAT138-1, LAT186-10, the LCL '-10' towers
(LCL148/306/339/348-10), and NUM284-10.014. Two of these now prove standalone
via the `unfold`/`unfold-tgt` lanes that landed after that sweep (COL002-5
0.02 s, KLE122-10 6.7 s -- both re-verified locally 2026-07-16), so the
next full sweep should reflect them.

The remaining binding constraints, measured:

- **Raw throughput + memory at 1M+ CP queues** (the GRP67x/69x cluster):
  aggressive slices hit the RSS guard or starve selection; the proof-trace
  and queue stores dominate memory.
- **Dual-ACI lattices** (LAT138-1): meet+join both AC+idempotent; our
  AC-completion explodes where ordered completion with
  ground-joinability-modulo-AC would breeze through.
- **Condensed-detachment generative towers** (the LCL residue): the CD lanes
  reshape the search but the deep '-10' family needs more.

The standing lesson from mining Vampire's source: its UEQ edge is **portfolio
breadth** -- ~400 auto-sampled single-strategy configs (ordering x symbol
precedence x selection x demodulation), dominated by LPO and
symbol-precedence-by-frequency, with no AC reasoning. thvm's complementary
edges are AC-completion (which Vampire lacks), the WM-faithful trajectory,
the exit-fast transform lanes, and encoded-Horn CD -- the union now outscores
Vampire on all three corpora at equal conditions (the standing table at the
top of this document).

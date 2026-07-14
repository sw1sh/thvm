# thvm CASC UEQ strategy: how FindProof proves unit-equality problems

This describes the strategy the `FindProof` CLI (`tools/thvm_atp_cli.c`) runs
for the CASC UEQ (Unit EQuality) division: the problem pipeline, the
concurrent slice portfolio, the underlying saturation engine, and the proof
story. Current standing (2026-07-13, 240s wall, pod hardware): CASC-30
279/300, versus Vampire's official 263 and Twee's 260.

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
`tp_preprocess` strips comments and splices one level of `include('Axioms/…')`
directives, resolving them against `$TPTP`. (A missing/unresolvable include is
now a hard `SZS status Error`, not a silent GaveUp — commit 4c969660.)

### 2.2 Problem transforms (all in the CLI, before the engine sees anything)
These run on the parsed clause set, each gated so it is a no-op when
inapplicable:

- **SInE axiom selection** (`tp_sine_select`): for oversized knowledge bases
  (CSR family, 8k+ clauses) goal-directed relevance pruning cuts the problem
  to the reachable slice before the capacity checks.
- **Wide-functor currying** (`tp_curry_wide`, `THVM_ATP_CURRY`): an n-ary
  functor with arity > 8 (e.g. TOP052's `tuple/16`) is decomposed into nested
  binary `ap2` applications so the arity-8 flatterm reader can load it. Sound
  bijective re-encoding.
- **Existential-goal equals encoding** (`tp_equals_goal`, `--equals-goal`): a
  Twee `addNarrowing`-style encoding of a non-ground goal via eq/true/false,
  for the existential family (COL009-1).
- **Encoded-Horn release** (`--horn`, `--horn-cd`, `--horn-back`): the
  condensed-detachment (LCL) problems are encoded Horn clauses; these run
  forward chaining, goal-directed backward CD with iterative-deepening DFS,
  and generalization-subsumption under Otter-style max-weight discipline.
- **Canonical input presentation** (`tp_canon_input`, `--canon-input`, default
  ON): orient every axiom heavy-side-left (by KBO weight, ties by canonical
  clause text under first-occurrence variable renaming) and stable-sort the
  clauses. This makes the emitted `.pr` **presentation-invariant** — the same
  theory presented three different ways (TPTP release variations) emits
  byte-identical input, so a problem does not flip solvable/unsolvable just
  because CASC shuffled the axiom order. (commit 4b2ea578.)

### 2.3 `.pr` emission
`tp_emit_pr` serializes the transformed problem into the Waldmeister `.pr`
format (weights, precedence, ORDERING section, equations, goal). The engine's
`.pr` ingestion path is byte-identical to the WL bench, so a CLI run with a
given knob set reproduces a bench run exactly.

## 3. The portfolio: concurrent slices

`FindProof --schedule` runs a **portfolio** of strategy slices concurrently
(`tools/thvm_atp_cli.c`, the `SCHEDULE[]` table). Each slice is a full
`FindProof` invocation with a distinct flag set, forked into its own process
group, each given the full wall clock; the **first slice to print
`SZS status Unsatisfiable` wins** and the rest are killed. Concurrency width is
`THVM_ATP_SCHED_MAX_PAR` (default 5; set to the slice count on a many-core box
so the whole portfolio races in one wall budget — commit cd1427e1). The slices,
and what each is for:

| slice | flags (essence) | targets |
|---|---|---|
| `wm` | the Waldmeister preset | the strongest general UEQ solver; most problems |
| `equals` | `--equals-goal=2` | existential goals (COL009-1); exits fast otherwise |
| `horn-cd` / `horn-back` | condensed detachment | encoded-Horn LCL family |
| `flatten` | `--flatten-goal --lazy-ground-join --backward-subsume --backward-demod --auto-max-weight` | deep-goal UEQ completion stalls on (COL060, GRP167) |
| `flatten-bare` | bare goal flattening | wall-edge flatten proofs the dressed extras push over the limit |
| `twee` | `--simpler-join --flatten-goal --lazy-ground-join --cp-weight=8 --w2-modulo=0 --orphan-murder=0 --cp-set-interreduce --cp-set-ir-budget=5` | the Twee-native completion regime (lattice/group residuals) |
| `mnf-gj-amw` | `--mnf --ground-join --connectedness --auto-max-weight` | goal-directed MNF front + ground join |
| `encompass` | `--encompass-demod` | AC/comm-heavy families that drown in variants (MVA003, LAT163) |
| `full-ac` | `--full-ac` | ring/lattice AC-completion (RNG021 family) — see §5 |
| `unfold` | `--def-unfold=2` + the flatten flags | pure function definitions `f(X..) = t` (Vampire definition_unfolding): the folded symbol is KBO-irreducible, so a folded goal is reachable only by presentation-dependent detours; unfolded, the goal's subterm cone flattens onto minimal constants and COL002-5's 6-superposition proof surfaces in 0.0s under any presentation — exits fast when no definition exists |
| `bare` | `--preset=none` | WM-preset-divergent problems (BOO014 ifeq-encoded) |
| `ifeq` | `--ifeq-guard=2 --flatten-goal --goal-subterm --preset=none` | guard-aware CP selection on the ifeq-encoded '-10' family; exits fast otherwise |
| `tgt` | `--tgt=1 --preset=none` | Vampire tgt / Twee goal transformation: name conjecture subterms (SYN615-10) |
| `twee-raw`, `bare-raw`, `flatten-bare-raw` | `--canon-input=0` variants | recover problems whose ORIGINAL presentation the canonical form loses |
| `wm-gj`, `lazy-gj`, `bare-gj` | ground-join backups | launch only when an early slice exits |

The portfolio is the design: no single strategy proves the whole corpus, so
diverse selection/ordering/redundancy regimes race, and each problem is won by
whichever regime fits it.

### 3.1 The FOF pack (`--fof-slice=2` slices)

On a `fof(` input every UEQ slice runs the IDENTICAL clausal front-end
(clausify + Horn release + or-lane + the 60%-of-remaining-wall clausal
pre-pass, `fof_clausal_prepass`) and differs only in engine knobs the pre-pass
never reads — measured on a 56-problem stratified FEQ+FNE sample at 60s, the
dominant failure class (26/56 Timeout) was one strategy racing itself. The FOF
pack varies the levers that DO matter for the front-end; each slice exits
immediately on non-`fof(` input (raw-text probe `tp_input_is_fof`, the
`equals`/`horn-cd` `=2` discipline), so on the UEQ corpus the pack costs one
fork + read per slice.

| slice | flags | front-end lever |
|---|---|---|
| `fof-fifo` | `--fol-age-ratio=1` | pre-pass given-clause selection: pure FIFO fairness |
| `fof-best` | `--fol-age-ratio=0` | pure best-first (no age interleave) |
| `fof-flat` | `--fol-goal-tier=0` | drop the conjecture-ancestry tier: flat weight order |
| `fof-deep` | `--fof-sat=1000000` | wall-bounded pre-pass (the default 20k-step cap, not the wall, starves SInE'd cones) |
| `fof-nopre` | `--fof-sat=0` | no pre-pass: the Horn/or equational lanes get the whole wall |
| `fof-or2` | `--fof-or=2` | eq-reified or-lane encoding (predicate atoms as `eq(p,true)`) |
| `fof-full` | `--fof-sine=0` | pre-pass on the FULL clausified set where the default SInE-cones it |

Two mechanisms ride along:

- **Pre-pass SInE** (`--fof-sine`, default ON above 512 clausified clauses):
  `fof_sine_cone` replays `tp_sine_pass`'s trigger discipline (generality
  threshold `gt=1`, rare-cap `rc=64`, tolerance ladder 1.5/1.2/1.0 at full
  trigger depth) at the `FolClause` level, so the bounded saturation works the
  goal-relevant cone instead of an 11k-clause knowledge base. Only the
  pre-pass input shrinks — the equational lanes still see every clause, and a
  subset refutation is still a refutation. When no tolerance fits the
  threshold the LOOSEST cone stands (there is no capacity cap to fit, and
  over-tightening provably loses refutations). A cone that SATURATES without
  refuting is a proven miss for that subset, so the pre-pass retries once on
  the full clause set under the remaining wall guard.
- **Oversized-clause drop** (`fof_ser_emit`/`tp_fof_push` soft mode): an
  axiom-side clause whose ifeq-encoded serialization blows the 2048-char
  `.pr` side bound is DROPPED instead of killing the run — the Mizar SET007
  families (ALG215+2, CAT034+2) carry a handful of giant definition clauses
  and every slice died on the first one. Dropping only shrinks the set;
  the goal-side pushes keep the hard give-up.
- **Pre-pass dedup**: every UEQ-tuned slice except `wm` now carries
  `--fof-sat=0`. Their pre-pass runs duplicated the `wm` slice's byte-for-byte
  (same clausified input, same selection) and burned up to 60% of the slice
  wall before the engine ever differed; on FOF input the pre-pass now runs
  only in the lanes that vary it.

## 4. The saturation engine (what a slice actually does)

Each slice runs the shared completion engine (`src/atp/_.c`, ~27k lines,
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
- The default `wm` path is **byte-faithful to real Waldmeister** — verified by
  `THVM_ATP_INSERT_TRACE` stream comparison against reference runs. All
  extensions are opt-in flags or separate slices, never changes to that path.

## 5. Borrowed mechanisms (the extensions over Waldmeister)

- **Twee's join discipline** (`--simpler-join`, `--cp-set-interreduce`,
  `--orphan-murder=0`): the `simplerThan`-gated join culls CPs plain
  joinability keeps; passive re-scoring on Twee's task discipline (freq 10s +
  5% budget) instead of an every-16-rules full sweep (3× the throughput);
  removing Waldmeister's Waisenmord (orphan murder) so parent-collapsed CPs
  survive to be re-scored the way Twee does. These flip the lattice/group
  residuals (LAT149, GRP177/753/766).
- **AC-completion** (`--full-ac`, commit 66a9e1c3): the engine has a full AC
  subsystem (AC-matching, Stickel AC-unification, AC-flatten/canon in
  `src/atp/ac.c`) that was compiled in but **inert** — nothing set the AC mask.
  `--full-ac` auto-detects AC operators (from their commutativity+associativity
  axioms) and runs AC-modulo completion. This flips the ring family (RNG021-6/7,
  025-5, 027-7, 029-5/6) that plain completion drowns on — the same problems
  Vampire solves in 0.3s, but by a *different* route (Vampire uses superposition,
  not AC).
- **Vampire's LPO ordering** (`--ord=lpo`, commit d731790c): LPO orients
  permutative laws KBO cannot. Wired but not yet in the schedule — realizing
  Vampire's LPO gains needs its per-problem precedence+selection tuning.
- **Encoded-Horn condensed detachment**: the LCL family is not equational
  completion at all; it is Horn-clause CD, run with forward/backward chaining
  and Otter-style max-weight + generalization-subsumption discipline.

## 6. The proof story

A winning slice can emit a full proof (`--proof`): `cli_proof_hook` sources the
goal from GC-rooted engine state, `cli_resolve_step_trace` replay-verifies each
citation in the trace domain, and treatment-compressed records are expanded
into single-rewrite steps at emission (Track C1, commit 04b77bc5) so every
emitted step is one verifiable rewrite. The independent checker
(`tools/proof_check.c`, `--check`) replays the proof: equational routes,
superposition replay for critical-pair records, unfailing-completion sessions
for the residue, per-step OOM containment. GRP706-1's 1024-record proof now
verifies end-to-end in default mode (0 abstains); the labelled corpus scores
138/138 with zero false-VALIDs.

## 7. Where Vampire still wins (the open gap)

On CASC-30, 10 of thvm's 21 misses are Vampire solves. Split by Vampire's time:

- **Fast (clean missing mechanism)**: LAT138-1 (0.5s, dual-ACI lattice —
  meet+join both AC+idempotent, our AC-completion explodes where Twee's ordered
  KB completion with ground-joinability-modulo-AC breezes through), SYN615-10
  (1.7s, congruence flooding), LCL306-10 (5s, condensed-detachment generative
  tower).
- **Slow (throughput/selection)**: GRP678/690/692/725 (44–157s), KLE122-10
  (66s) — the right regime exists but does not reach the proof in budget.

The standing lesson from mining Vampire's source: its UEQ edge is **portfolio
breadth** — ~400 auto-sampled single-strategy configs (ordering × symbol
precedence × selection × demodulation), dominated by LPO and
symbol-precedence-by-frequency, with *no* AC reasoning. thvm's complementary
edges are AC-completion (which Vampire lacks), the WM-faithful trajectory, and
encoded-Horn CD. Closing the remaining gap means either matching that breadth
(a config sampler over our knobs) or, per-problem, porting the specific
mechanism a fast competitor uses — which is the current grind.

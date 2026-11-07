# ATP: automated theorem proving

`src/atp/` is thvm's equational theorem prover. Given equational axioms
and a goal equation, it searches for a proof by unfailing Knuth-Bendix
completion: it orients axioms into rewrite rules under a reduction
ordering, forms critical pairs (CPs) by superposition, normalizes them,
and checks whether the goal's two sides reach a common normal form. The
default trajectory follows Waldmeister step for step; mechanisms ported
from Twee, Vampire and Prover9, associative-commutative (AC) completion,
the condensed-detachment lanes, the first-order (FOF) clausal front end
and the learned selection layer are flags and schedule entries on top.

Two surfaces drive it:

* **`bin/FindProof`** (`tools/thvm_atp_cli.c` over `tools/atp_cli/`,
  `make bin/FindProof`): the CLI over TPTP input and the CASC (CADE ATP
  System Competition) entry. `bin/FindProof --check` is the proof checker
  (`tools/proof_check.c`), entered in ProoVer as CheckProof.
* **`TFindProof`** in the THVMLink paclet (`wl/THVMLink/Kernel/ATP/`),
  which returns a `ProofObject`. Its methods and options are documented
  in the paclet's pages (`wl/THVMLink/docs/Symbols/TFindProof.md`,
  `wl/THVMLink/docs/Tutorials/AtpMethods.md`).
* **`TKnuthBendixCompletion`** in the same paclet, which runs the
  completion without a goal and returns the completed system, or the
  normal form of a term under it: the term is run as the goal
  `term == c` with `c` a fresh constant, and the bridge returns each goal
  side normalized under the final rules by `thvm_atp_normal_form`
  (`src/atp/normal_form.c`). Its page is
  `wl/THVMLink/docs/Symbols/TKnuthBendixCompletion.md`.
* **`TAtpComplete`** in the same paclet completes once and keeps the
  engine state for many queries (`TAtpNormalForm`, `TAtpEqualQ`), the
  handle v2's FullSimplify drives. The bridge keeps the state under a
  managed `AtpSystem` expression with its ordering copied out of the
  run's static config, and answers through the flatterm normalizer,
  which reads only the off-heap rule mirror, so other runs collecting
  the shared heap leave it intact. A completion stops at `MaxSteps` or
  at an allocation budget in heap cells (`thvm_atp_set_max_alloc`),
  both machine-independent.
* **`WolframInstitute`ATP``** (`wl/ATP/`) is the same handle as a
  standalone package over its own library, `libthvm_atp` (one TU over
  the runtime and `thvmlink_atp.c`, 1.4 MB), for v2's SystemLibrary
  until v2 ships THVMLink whole; then it is deleted. `wl/ATP/README.md`
  has its build and the determinism harness.

At CASC-J13 (2026) FindProof placed 4th of 10 in UEQ (unit equality,
269 of 400) and 11th of 17 in FOF (91 of 400); CheckProof placed last in
ProoVer. [casc_j13_postmortem.md](casc_j13_postmortem.md) has the
analysis.

## Documents

* [casc_strategy.md](casc_strategy.md): how FindProof proves competition
  problems: the problem pipeline, the transforms, the concurrent slice
  schedule, the engine loop, the proof output.
* [engineering.md](engineering.md): how the algorithms map to data
  structures: rule storage, the CP queue, discrimination trees, memos,
  flatterms, the build defines and setters, soundness probes, profiling.
* [algorithms.md](algorithms.md): the mathematics the engine implements:
  completion, orderings, matching and unification, redundancy criteria.
* [env_knobs.md](env_knobs.md): the `THVM_ATP_*` environment knobs.
* [starexec.md](starexec.md): building and running the competition
  tarballs (FindProof and CheckProof) the way StarExec does.
* [cli_process_methods.md](cli_process_methods.md): the
  `Method -> "<X>Process"` bridge to the external provers and the shared
  proof lift.
* [vampire_port.md](vampire_port.md): Vampire and Prover9 options against
  their thvm equivalents.
* [guide.md](guide.md): the ENIGMA selection layer (`--guide`,
  `src/atp/guide.c`): integrations, inputs, compiled-in heads.
* [enigma_portfolio.md](enigma_portfolio.md): the pre-registered plan for
  learned selection against the CASC portfolio, with its frozen DEV-S
  list [enigma_devs.tsv](enigma_devs.tsv). Changes to it are dated
  amendments only.
* [ml_guidance.md](ml_guidance.md): the learned CP scorers behind
  `Method -> "ENIGMA"` and the GNN scorer.
* [atp_neural_selection.md](atp_neural_selection.md): the derivation-history
  design behind `src/atp/history.c` and its phase 0c results.
* [unification_roadmap.md](unification_roadmap.md): a design for unifying
  SMT (CDCL(T), theories) with completion.
* [prover_landscape.md](prover_landscape.md): a survey of the field's
  provers and their techniques.

## Code map

The engine is one translation unit: `src/atp/_.c` includes its modules in
build order.

| Concern | Files in `src/atp/` |
| --- | --- |
| state, the step loop, the run's lifecycle | `_.c`, `state.c`, `step.c`, `lifecycle.c`, `gc.c` (heap checkpoints, collection, trace materialization) |
| critical pairs: formation, emission, queue, weights | `cp_gen.c`, `cp_emit.c`, `cp_push.c`, `cp_store.c`, `cp_heap.c`, `cp_weight.c`, `overlap_index.c` |
| redundancy | `cp_interreduce.c`, `subsume.c`, `fv_index.c`, `ground_join.c` |
| rules and normalization | `rule_index.c`, `norm_paths.c`, `ordered_rewrite.c`, `lpo_cache.c` |
| the goal | `goal_check.c`, `goal_incr.c`, `goal_narrow.c`, `narrow.c`, `mnf.c` (Waldmeister's multiple-normal-forms goal search) |
| proofs | `trace.c`, `rule_journal.c`, `proof_cone.c` |
| flatterms | `ft.h`, `ft.c`, `ft_alloc.c`, `ft_match.c`, `ft_unify.c`, `ft_order.c`, `ft_norm.c`, `ft_splice.c`, `ft_ri.c`, `ft_cp.c`, `ft_cpq.c`, `ft_ac_eq.c`, `ft_ac_match.c` |
| Waldmeister fidelity | `wm_order.c` (CP emission order), `wm_intake.c` (axiom intake), `wm_drain.c` (interreduction order) |
| AC, precedence, Otter-loop passive demodulation | `ac.c`, `precedence.c`, `otter.c` |
| learned selection | `enigma_data.c`, `gnn.c`, `rerank.c`, `topk.c`, `guide.c`, `guide_models.c` (generated), `history.c` |
| diagnostics, IC primitives | `diag.c`, `prim.c` |

Outside `src/atp/`: the term-side orderings (`src/kbo/`, `src/lpo/`,
`src/rpo/`, `src/wpo/`), unification, critical pairs and rewriting
(`src/unify/`, `src/cp/`, `src/rewrite/`), the FOF clause core and
clausal saturation (`src/fol/`), congruence closure (`src/cc/`),
finite-model enumeration (`src/ffmep/`), the Waldmeister normalization
port (`src/wmfpa/`) and the `.pr` parser (`src/wald/`).

The CLI: `tools/atp_cli/main.c` holds `main` and the slice schedule
`SCHEDULE[]`, `knobs.c` the `KNOBS[]` table; the TPTP side is
`tptp_ueq.c`, `tptp_include.c` and `canon_input.c`; the problem
transforms are `curry.c`, `def_unfold.c`, `equals_encode.c`; the FOF and
Horn lanes are `fof_prepass.c`, `fof_split.c`, `horn_back.c`,
`horn_release.c` and `ifeq10.c`; `szs.c` and `provenance.c` write the SZS
output and the proof; `problem_features.c` and `lane_gates.c` compute the
problem features and the static gates of the ENIGMA lanes. The engine
layer it compiles is `tests/atp/test_atp_wolfram_bench.c`.

## Entry points

```c
AtpState *s = thvm_atp_init(kbo_cfg, /*step_cap=*/100000);
thvm_atp_set_lpo(s, lpo_cfg);          // optional: LPO mode
thvm_atp_add_equation(s, lhs, rhs);    // axioms
thvm_atp_set_goal(s, gl, gr);          // proof goal

while (thvm_atp_step(s) == ATP_RUNNING) { /* ... */ }
// then ATP_PROVED, ATP_REFUTED, ATP_TIMEOUT, ATP_QUEUE_EMPTY or ATP_ABORTED
```

`thvm_atp_step` (`src/atp/rerank.c`) pops the best CP, normalizes both
sides, joins it or adds it as a rule, then interreduces and checks the
goal. From the CLI (`TPTP` names the directory holding `Axioms/`):

```sh
make bin/FindProof
TPTP=~/.cache/thvm/casc ./bin/FindProof --schedule --szs --proof --wall=30 problem.p
./bin/FindProof --check --szs proof.p
```

## Tests

`tests/atp/` holds the track's tests (built by [atp.mk](../../atp.mk) and
listed in `ATP_TESTS`), its shell gates (`atp_*.sh`), the diagnostic
probes (`make probes`), the `.pr` problems of `tests/atp/data/` and the
FindProof soundness corpus (`tests/atp/soundness/`). `bin/test_atp` is
the engine regression suite; `test_atp_ft*`, `test_ft*`, `test_ac` and
`test_lpo_cache` cover the flatterm stack, AC and the caches; the
`*_deep` tests pin the deep-term walks; `test_atp_wolfram_bench` runs one
`.pr` problem (`tools/baselines/wm_pr/`) and is what the Waldmeister pins
measure:

```sh
make bin/test_atp_wolfram_bench
THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=1 ./bin/test_atp_wolfram_bench \
    tools/baselines/wm_pr/WolframAxioms__OrAssociativity.pr 500000 280
```

# thvm/atp -- Automated Theorem Proving

`src/atp/` is thvm's equational theorem prover. Given a set of
equational axioms and a goal equation, it attempts a proof by unfailing
Knuth-Bendix completion: orient each axiom into a rewrite rule under a
reduction ordering, generate critical pairs (CPs) by superposition,
normalize them, and check whether the goal's two sides reach a common
normal form. The default trajectory is byte-faithful to Waldmeister
(the historically dominant unit-equality prover); everything beyond
that -- mechanisms ported from Twee and Vampire, an
associative-commutative (AC) completion mode, condensed-detachment
lanes, a first-order-logic (FOL) clausal front end -- is opt-in flags
and portfolio slices layered on top.

The prover has two user surfaces:

* **`bin/FindProof`** (`tools/thvm_atp_cli.c`, built by
  `make bin/FindProof`): a self-contained CLI over TPTP input. This is
  the CASC competition entry; see
  [casc_strategy.md](casc_strategy.md) and [starexec.md](starexec.md).
* **`TFindProof`** in the Wolfram Language (WL) paclet
  (`wl/THVMLink/Kernel/ATP/`): returns a real `ProofObject`. See
  `docs/tutorial/atp_methods.md` and
  [cli_process_methods.md](cli_process_methods.md).

## Current standing (as of 2026-07-16)

Unit EQuality (UEQ) division, 300 problems per corpus, 240 s wall on
pod hardware (48-core Linux, `THVM_ATP_SCHED_MAX_PAR` = slice count so
the whole portfolio races in one wave; result TSVs under
`~/.cache/thvm/casc/pod_results/`):

| corpus | thvm | official winner | Vampire 5.0.1 from source, same pod + wall |
|---|---|---|---|
| CASC-30 (2025) | 281 | Vampire 263 | 269 |
| CASC-29 (2023) | 282 | Twee 2.4.1 260 | 271 |
| CASC-J12 (2024) | 279 | Vampire 4.9 265 | 273 |

thvm beats every official UEQ winner and the from-source Vampire
baseline on all three corpora. (Official numbers are from 120 s
StarExec competition runs; the pod comparison against Vampire is
like-for-like at 240 s on identical hardware.)

First-Order Form (FOF): the day-1 hybrid pipeline (clausification +
Horn/or equational encoding + a bounded clausal-saturation pre-pass)
proves 112/500 of a stratified FEQ+FNE sample at 240 s
(84/400 FEQ + 28/100 FNE, `pod_results/fof_*_240.tsv`). This is a new
lane, not yet a competitive FOF entry.

## Documentation map

Living documents (kept current with the code):

* **[casc_strategy.md](casc_strategy.md)** -- how `FindProof` proves
  CASC problems: the problem pipeline, the transform inventory, the
  concurrent slice schedule, the engine loop, the proof story.
* **[engineering.md](engineering.md)** -- how the algorithms map to
  data structures: rule storage, CP queue, discrimination trees,
  memos, the flatterm representation, controls (build defines, C
  setters, env knobs), soundness probes, profiling.
* **[starexec.md](starexec.md)** -- building and running the
  competition tarballs (FindProof + CheckProof) the way StarExec does.
* **[cli_process_methods.md](cli_process_methods.md)** -- the WL
  `Method -> "<X>Process"` bridge to external provers
  (Vampire/Twee/Waldmeister/E) and the shared proof-object lift.
* **`docs/tutorial/atp_methods.md`** -- the WL `TFindProof` Method /
  suboption catalog (the user-facing knob surface).
* **`docs/ENV_KNOBS.md`** -- the repo-wide env-variable index; the ATP
  section points back here.

Historical snapshots (design notes and closed investigations; knob
names there may predate the current CLI):

* **[algorithms.md](algorithms.md)** -- the math the engine implements:
  completion, orderings, matching/unification, redundancy criteria.
* **[roadmap.md](roadmap.md)**, **[survey.md](survey.md)** -- coverage
  and peer-prover positioning at the time they were written.
* **[ml_guidance.md](ml_guidance.md)**, **[gnn_pipeline.md](gnn_pipeline.md)**
  -- the ENIGMA-style learned CP selection arc (see also
  `tools/atp_enigma/README.md` for the current dataset tooling).
* **[vampire_port.md](vampire_port.md)**,
  **[vampire_case_teardown.md](vampire_case_teardown.md)** -- the
  Vampire flag-mapping and per-problem teardown studies.
* **[unification_roadmap.md](unification_roadmap.md)**,
  **[unification_phase01_plan.md](unification_phase01_plan.md)** -- the
  SMT-unification design arc.
* **[proof_parity_waldmeister.md](proof_parity_waldmeister.md)**,
  **[proof_parity_inverseofinverse.md](proof_parity_inverseofinverse.md)**,
  **[cli_lift_status.md](cli_lift_status.md)**,
  **[wolfram_assoc_wl_hang.md](wolfram_assoc_wl_hang.md)** -- WL-side
  proof-object parity and lift studies.

## Code coverage: what lives where

Line counts as of 2026-07-16 (`wc -l`). The engine proper is a single
translation unit: `src/atp/_.c` directly `#include`s `lpo_cache.c`,
`ac.c`, `goal_narrow.c`, `wm_order.c`, and `wm_intake.c`, so their
"entry points" are internal functions the saturation loop calls; the
`ft_*.c` files compile separately. The CLI compiles the whole engine
into one binary via
`tools/thvm_atp_cli.c -> tests/test_atp_wolfram_bench.c -> src/thvm.c`.

| file | lines | responsibility | main entry points |
|---|---|---|---|
| `src/atp/_.c` | 30812 | the saturation engine: rule storage, CP queue + selection, interreduction, indexes, memos, goal check, proof trace, rule-set journal (`rule_journal.c`: every rule birth, content and death, for the `--proof` re-derivation), GC roots | `thvm_atp_init`, `thvm_atp_add_equation`, `thvm_atp_set_goal`, `thvm_atp_step`, `thvm_atp_free`, the `thvm_atp_set_*` knob setters |
| `src/atp/ft.c` | 777 | flatterm (AtpFt) cell representation: constructors, Term converters, equality/hash, pretty-print | `ftnew_var/const/ctr`, `ft_from_term`, `ft_to_term`, `ft_eq`, `ft_hash` |
| `src/atp/ft_alloc.c` | 425 | flatterm arenas: persistent slab pool + bump-pointer scratch | `ft_alloc_persistent`, `ft_free_span`, `ft_scratch_reset` |
| `src/atp/ft_match.c` | 405 | flatterm matching + substitution application | `ft_match`, `ft_subst_apply` |
| `src/atp/ft_unify.c` | 199 | flatterm unification (CP formation side) | `ft_unify` |
| `src/atp/ft_norm.c` | 4865 | the flatterm normalizer: innermost rewriting fixpoint, treated-CP classes, Waldmeister flat-path emulation, the goal walk's record and replay (`goal_incr.c`) | `atp_rewrite_normalize_ft` |
| `src/atp/ft_splice.c` | 431 | in-place rewrite at a redex position (Waldmeister `SigmaRInEZ`/`InLZ` port) | `ft_splice` |
| `src/atp/ft_order.c` | 838 | Knuth-Bendix Ordering (KBO) + Lexicographic Path Ordering (LPO) over flatterm cells | `thvm_kbo_ft`, `thvm_lpo_ft` |
| `src/atp/ft_ri.c` | 399 | discrimination-tree descent walking flatterm cells | `atp_ri_descend_ft_at` |
| `src/atp/ft_cp.c` | 410 | single-walk native critical-pair formation (the Waldmeister CP-walk port) | `vcp_ft_*` |
| `src/atp/ft_cpq.c` | 319 | flatterm CP-queue store (decode-free pop, native joinability) | `atp_cp_trivially_joinable_ft` |
| `src/atp/ft_ac_eq.c` | 110 | AC-equality on flatterms | `ft_ac_eq` |
| `src/atp/ft_ac_match.c` | 434 | AC matching on flatterms | `ft_ac_match` |
| `src/atp/ac.c` | 974 | the AC subsystem: AC-operator detection, AC-canonical flatten, Stickel AC-unification | `thvm_atp_auto_ac`, `thvm_atp_set_ac_mask` |
| `src/atp/goal_narrow.c` | 695 | ground congruence-closure + goal-cone e-matching endgame (`--goal-narrow`) | `gn_*`, switched by `thvm_atp_set_use_goal_narrow` |
| `src/atp/precedence.c` | 976 | automatic precedence from algebraic-structure analysis (Waldmeister PhilMarlow port), the Vampire `sp=` / `spb=goal` / LADR precedence ports, KBO weight schemes, and the floor that keeps the reserved grounding constants below every user symbol | `atp_auto_precedence`, `atp_analyze_axioms`, `atp_vampire_precedence_g`, `atp_precedence_floor_reserved` |
| `src/atp/goal_incr.c` | 474 | incremental goal check: the single universal goal keeps its last full normalization while a replay of its walk against the rules added since shows a full normalization would retrace it | `atp_goal_incr_same`, `atp_goal_incr_store` (called from the goal check) |
| `src/atp/lpo_cache.c` | 139 | LPO orientability cache (build-gated) | `atp_lpo_orient_cache_invalidate`, `thvm_atp_orient_stats` |
| `src/atp/wm_intake.c` | 462 | Waldmeister intake ordering / CP classification | `thvm_atp_wm_intake_order` |
| `src/atp/wm_order.c` | 2989 | Waldmeister CP-selection order: MixWeight comparator, rank trie, N-way rank cache | `atp_wmo_insert_fact`, `wmo_*` |
| `src/fol/_.c` | 595 | FOL clause core: clause store, resolution, factoring, paramodulation | `fol_clause_new`, `fol_resolve`, `fol_factor`, `fol_paramodulate` |
| `src/fol/cnf.c` | 592 | clausification: negation normal form, skolemization, distribution with Tseitin fallback | `fol_nnf`, `fol_skolemize`, `fol_formula_to_clauses`, `fol_formula_to_clauses_tseitin` |
| `src/fol/sat.c` | 1172 | bounded clausal-saturation pre-pass (given-clause loop with age/goal-tier selection) | `cnf_run` |
| `tools/thvm_atp_cli.c` | 8317 | the `FindProof` CLI: TPTP parsing, problem transforms, `.pr` emission, the slice schedule, SZS output, proof emission | `main`, `tp_*` transforms, `fof_clausal_prepass`, `cli_proof_hook`, `SCHEDULE[]`, `KNOBS[]` |
| `tools/proof_check.c` | 1868 | the independent proof checker (`FindProof --check`, the ProoVer CheckProof entry) | `proof_check_main` |
| `tests/test_atp.c` | 6935 | the engine regression suite (~136k assertions) | `make test_atp` |
| `tests/test_atp_wolfram_bench.c` | 3599 | the bench harness: env-knob glue, `.pr` ingestion, corpus problems; the CLI includes it as its engine layer | `make test_atp_wolfram_bench` |

Supporting modules outside `src/atp/`: `src/kbo/_.c` and `src/lpo/_.c`
(the Term-side reduction orderings), `src/rewrite/_.c` (Term match +
substitution), `src/unify/_.c` (Term unification), `src/cp/_.c`
(Term-side CP enumeration). The flatterm lanes above are the default
path; the Term-side modules remain as the reference implementation and
for the WL bridge.

## Entry points

```c
AtpState *s = thvm_atp_init(kbo_cfg, /*step_cap=*/100000);
thvm_atp_set_lpo(s, lpo_cfg);          // optional: LPO mode
thvm_atp_add_equation(s, lhs, rhs);    // axioms
thvm_atp_set_goal(s, gl, gr);          // proof goal (single equation)

while (thvm_atp_step(s) == ATP_RUNNING) { /* ... */ }
// terminal status: ATP_PROVED, ATP_DISPROVED, ATP_SATURATED, ATP_BUDGET
```

The driver is `thvm_atp_step` in `src/atp/_.c`: it pops the
top-priority CP, normalizes both sides, joins or commits-as-rule, then
runs interreduction and the goal check.

CLI equivalent (verified 2026-07-16; `TPTP` points at the directory
containing `Axioms/`):

```sh
make bin/FindProof
TPTP=~/.cache/thvm/casc ./bin/FindProof --schedule --szs --proof --wall=30 \
    ~/.cache/thvm/casc/Problems30/UEQ/LAT171-1.p
# -> % SZS status Unsatisfiable ... + a CNFRefutation block (0.3 s)
./bin/FindProof --check --szs proof.p    # -> % SZS status VerifiedGood
```

## Test suite

`tests/test_atp.c` is the regression baseline (~136k assertions,
default build). Feature-gated binaries cover the flatterm stack, the
AC subsystem, and the opt-in caches -- the full list is the
`test_atp*` / `test_ft*` targets in the `Makefile`; the most-used ones:

    make test_atp                 # engine regression suite
    make test_atp_ft_norm_verify  # live Term-vs-flatterm differential
    make test_ft_cpq              # flatterm CP-queue invariants
    make test_ac                  # AC declarations + canonical flatten
    make test_lpo_cache           # LPO orient-cache differential

The strongest soundness gate is `bin/test_atp_ft_norm_verify` --
the suite built with the full flatterm stack plus
`THVM_ATPFT_NORM_VERIFY=1`, which runs every push-normalization through
both the Term path and the flatterm path and aborts on disagreement.

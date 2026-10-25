# tools/baselines/ -- ATP reference problems and results

Problem files the ATP tests and tools read, the generators that rebuild
them, and the reference results thvm is compared against.

## Problems

* **`wm_pr/`** -- Waldmeister `.pr` files, one per `<Theory>__<Theorem>`
  of the `AxiomaticTheory` notable theorems (and `__c<k>` for one conjunct
  of a multi-conjunct theorem). Read by the C tests
  (`tests/atp/test_atp_session_reuse.c`, `tests/atp/atp_lane_cli.sh`,
  `tests/atp/atp_guide_gates.sh`), the Makefile's pin targets and the WL
  `"WaldmeisterProcess"` method (`wl/THVMLink/Kernel/ATP/ProcessProofObject.wl`).
* **`vampire_tptp/`**, **`vampire_raw/`** (gitignored, regenerable) --
  TPTP CNF files for the same problems and Vampire's raw output. The WL
  `"VampireProcess"`, `"TweeProcess"` and `"EproverProcess"` methods read
  `vampire_tptp/` on the named-theorem form. Rebuild both with
  `run_vampire_baseline.sh`.
* **`all_theorems.txt`** -- the `Theory::Theorem` list
  `run_vampire_baseline.sh` exports.

## Generators

* **`run_vampire_baseline.sh`** -- exports TPTP files for every notable
  theorem (through `tools/vampire/export_all.wls`), runs Vampire on each,
  and writes `vampire_notable_theorems.tsv`. One Vampire process at a time.
* **`tptp_to_pr.wls`** -- TPTP CNF to Waldmeister `.pr`, by direct string
  parsing (no `Wolfram`Parser` dependency), KBO with a precedence by arity:

  ```
  wl -f tools/baselines/tptp_to_pr.wls tools/baselines/vampire_tptp/<Theory>__<Thm>.p \
      tools/baselines/wm_pr/<Theory>__<Thm>.pr
  ```

## Reference results

* **`thvm_notable_theorems.tsv`**, **`wl_notable_theorems.tsv`**,
  **`vampire_notable_theorems.tsv`** -- status, time and proof length of
  every notable theorem under thvm, `FindEquationalProof` and Vampire.
  `tools/atp_enigma/build_all_safe.sh` takes its problem list from the thvm
  table.
* **`proofs/`** -- `FindEquationalProof` proofs of the notable theorems
  (`<Theory>__<Theorem>.m`), the manifest `tools/atp_enigma/gen_tier2_data.sh`
  reads.
* **`compare_vampireueq_vs_vampire.tsv`** -- the `"VampireUEQ"`
  configuration against the Vampire program, per theorem.
* **`wm_align_matrix.tsv`**, **`wm_align_reports/`** -- the Waldmeister
  alignment matrix (`tools/wm_align_sweep/`) and its per-problem selection
  reports.
* **`wm_order_sim/`** -- the Python mirror of Waldmeister's critical-pair
  emission order that `src/atp/wm_order.c` was validated against.
* **`trace_diff_agioc.md`** -- the thvm and Waldmeister trajectory diff on
  AbelianGroupAxioms InverseOfComposite that traced thvm's critical-pair
  weight against Waldmeister's.

## Runners

* **`run_atp_tests.sh`** -- runs every ATP `wl/THVMLink/Tests/*.wlt` file in
  its own kernel and reports the totals.
* **`probe_safely.sh`** -- runs a `wolframscript` probe under a heap cap and
  a hard wall-clock limit:

  ```
  tools/baselines/probe_safely.sh 60 probe.wls [args...]
  ```

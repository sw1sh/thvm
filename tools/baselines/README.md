# tools/baselines/ -- ATP comparison + sweep tooling

Drivers for comparing thvm's ATP engine against external CLI provers,
generating .pr/.p problem files, sweeping per-step parity, and plotting.

## ATP-CLI comparison (this session's work)

* **`diff_one_case.wls`** -- single-case parity diff between a thvm
  preset proof and the corresponding CLI ProofObject.  Emits one
  TSV row.  Designed for shell-loop parallelism + memory isolation
  (each invocation gets its own kernel).
  ```
  diff_one_case.wls THEORY THM PRESET [wm|vampire]
  ```

* **`diff_proofobject_lib.wl`** -- shared canon library used by
  both `diff_one_case.wls` and `diff_proofobject.wls`.  Exports
  `THVMBaselines\`runDiff[poP, poC]` which returns the
  per-step diff Association.

* **`diff_proofobject.wls`** -- sweep version: walks all 12 cases
  (8 easy + 4 hard) x 3 presets (Waldmeister + Automatic +
  VampireUEQ).  Writes `parity_perstep.tsv` + the per-case
  match-rate plot.  Uses incremental TSV writes
  (`parity_perstep_partial.tsv`) so a mid-sweep crash leaves
  partial data.

* **`lift_telemetry.wls`** -- across 8 easy cases, probes the
  lifted ProofObject's property dispatch:
    - Head -> ProofObject
    - ProofFunction -> Function
    - ProofGraph -> Graph
    - ProofLength -> Integer
    - Verify[Theorems] -> Success | Failure
  Output: `lift_telemetry.tsv`.

* **`metadata_diff.wls`** -- side-by-side preset vs CLI Proof
  field diff per construct key.  Used during the verification
  investigation to find which specific reconstructed fields
  mismatch.
  ```
  metadata_diff.wls THEORY THM
  ```

* **`lift_pipeline_test.wls`** -- diagnostic that confirms the
  lift's structural wrap (`ProofObject["EquationalLogic", goal,
  axioms, data]`) preserves verification when fed preset data.
  Isolates verification failures to specific reconstructed field
  values, not the wrap itself.

## Plot generators

* **`plot_atp_parity.wls`** -- consolidated 3-CLI bar chart
  (preset construct count + delta vs CLI for WM / Vampire / Twee).
  Writes `plots/atp_parity.png`.

* **`plot_perstep_parity.wls`** -- per-step match-rate bar chart
  across 12 cases x 3 presets.  Writes
  `plots/perstep_match_rate.png`.

## Probe wrapper

* **`probe_safely.sh`** -- `exec timeout` wrapper for interactive
  wolframscript probes that risk hanging thvm's C engine (per
  `docs/atp/wolfram_assoc_wl_hang.md`).  Sets a sensible heap
  cap (`THVM_HEAP_CELLS=512M`) and a hard wall-clock budget.
  Docstring includes a `ps + xargs` reaper one-liner for orphan
  kernels surviving SIGTERM grace.
  ```
  probe_safely.sh 60 diff_one_case.wls AbelianGroupAxioms \
      InverseOfInverse Waldmeister
  ```

## Sweep drivers

* **`sweep_hard_tier.sh`** -- runs `diff_one_case.wls` over the
  4 hard-tier theorems (WolframAxioms And/Or/Commut + Meredith
  And/OrAssoc + others) x 3 presets, one kernel per cell.
  Output: `parity_perstep_hard.tsv`.

## TPTP -> Waldmeister .pr converter

* **`tptp_to_pr.wls`** -- direct string-parsing TPTP CNF -> WM
  `.pr` converter (no `Wolfram\`Parser\`TPTPImport` dependency
  -- that EBNF-fetch path stalled for hours on first-run).
  KBO ordering with auto-precedence by arity + alphabetical.
  ```
  tptp_to_pr.wls tools/baselines/vampire_tptp/<Theory>__<Thm>.p \
      tools/baselines/wm_pr/<Theory>__<Thm>.pr
  ```

## Output files committed to repo

* `parity_perstep.tsv` -- per-step parity sweep (3 presets x 12 cases)
* `parity_perstep_hard.tsv` -- hard tier sweep
* `parity_wm_wmcli.tsv` -- WM preset vs WaldmeisterProcess parity
* `parity_vampire.tsv` -- Vampire preset vs VampireProcess parity
* `parity_twee.tsv` -- Twee preset vs TweeProcess parity
* `lift_telemetry.tsv` -- LiftToProofObject status across 8 cases
* `plots/atp_parity.png` -- 3-CLI consolidated bar chart
* `plots/perstep_match_rate.png` -- per-step match-rate plot
* `wm_pr/` -- pre-generated WM .pr files for the 8 easy + 4 hard
  AbelianGroup/Group/Boolean/Hillman/Meredith/Wolfram cases

## Related docs

* `docs/atp/cli_lift_status.md` -- the CLI -> ProofObject lift's
  achievement + verification gap
* `docs/atp/cli_process_methods.md` -- TFindProof Method->"*Process"
  tutorial
* `docs/atp/wolfram_assoc_wl_hang.md` -- the WL TFindProof hang on
  WolframAxioms/AndAssoc + safe-probe pattern

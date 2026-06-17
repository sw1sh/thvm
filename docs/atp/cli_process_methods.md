# CLI-process Method options on TFindProof

`TFindProof[thm, theory, Method -> "<X>Process"]` routes the proof
through an external ATP CLI binary instead of thvm's internal C
saturation engine, and lifts the resulting SZS / TSTP output into
the same thvm-shaped Association (or `ProofObject` head, with
`"LiftToProofObject" -> True`) that the internal presets produce.

This lets a comparator, ProofFunction verifier, or dataset reader
see one shape across both paths.

## The four Process methods

| Method                | Binary path                         | Strategy                          | Brew                |
| --------------------- | ----------------------------------- | --------------------------------- | ------------------- |
| `"VampireProcess"`    | `/opt/homebrew/bin/vampire`         | `--mode casc --proof tptp`        | `brew install vampire` |
| `"TweeProcess"`       | `~/.cabal/bin/twee` or `/opt/homebrew/bin/twee` | `--tstp --quiet`                  | `cabal install twee` |
| `"WaldmeisterProcess"`| `$WMCLI` env var                    | wmcli on a pre-generated `.pr` file | (build from source)  |
| `"EproverProcess"`    | `/opt/homebrew/bin/eprover`         | `--auto-schedule --proof-object --tstp-format` | `brew install eprover` |

All four route to a per-CLI `T<Name>ProofObject` builder in
`wl/THVMLink/Kernel/ATP/ProcessProofObject.wl`, which:

1. Calls the per-CLI wrapper (`TVampireProof` / `TTweeProof` /
   `TWaldmeisterProof` / `TEproverProof`) to invoke the binary
   and parse its output into a normalized Inference List.
2. Threads through `TSZSDerivationToProofObject` (a CLI-agnostic
   SZS→thvm-shape Association builder) which assigns construct
   keys (`{"Axiom", n}` / `{"Hypothesis", n}` / `{"CriticalPairLemma", n}`
   / `{"SubstitutionLemma", n}` / `{"Conclusion", 1}`) per the
   shared `$SZSRuleToConstruct` mapping.
3. Optionally lifts to a literal `ProofObject["EquationalLogic",
   goal, axioms, data]` head via `liftToProofObject` (see
   docs/atp/cli_lift_status.md for the achievement + verification
   gap).

## Shared options

All four `T<Name>ProofObject` builders accept the same option set:

* `TimeConstraint -> 30` -- wall-clock budget passed to the CLI.
* `"Binary" -> Automatic` -- absolute path override; Automatic
  walks the per-CLI binary-discovery list.
* `"ParseFormulas" -> False` -- when True, the per-step Statement
  field is `TPTPImport`-parsed into WL expression form (via
  `Wolfram\`Parser\`TPTPImport`).  Slow (~5s/formula on a
  multi-step proof) but enables structural comparison.
* `"LiftToProofObject" -> False` -- when True, wraps the
  Association into a literal `ProofObject[...]` head (implies
  `ParseFormulas -> True`).  Property accessors work
  (`ProofFunction` / `ProofGraph` / `ProofLength` / `Theorems`);
  `pf[Theorems]` full verification still fails on some entries
  per cli_lift_status.md.

## Skip-through behavior

When the underlying CLI binary isn't installed, the
`T<Name>ProofObject` builder returns
`Failure["ExternalNoProof", <|"Tool", "Status", "Seconds"|>]`
rather than raising an error.  The corresponding `.wlt` tests
(`atp_vampire.wlt` / `atp_twee.wlt` / `atp_eprover.wlt` / and
the cross-builder `atp_lift.wlt`) detect binary availability via
`FileExistsQ` / `$WMCLI` env var and skip past the assertion
when missing -- so a fresh-checkout CI doesn't break just
because eprover hasn't been brew-installed.

## Verify the dispatch

```
PacletDirectoryLoad["wl/THVMLink"]
Get["WolframInstitute`THVMLink`ATP`"]

(* Internal preset -- thvm C engine *)
p1 = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "Waldmeister"]
Head[p1]            (* -> ProofObject *)

(* Through wmcli, lift to ProofObject *)
p2 = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "WaldmeisterProcess",
    "LiftToProofObject" -> True]
Head[p2]            (* -> ProofObject *)
p2["ProofFunction"] (* -> Function *)

(* Through Vampire CLI (no lift -- raw Association) *)
p3 = TFindProof["InverseOfInverse", "AbelianGroupAxioms",
    Method -> "VampireProcess"]
Head[p3]            (* -> Association *)
p3["Backend"]       (* -> "SZS" *)
```

## Related docs

* docs/atp/cli_lift_status.md -- LiftToProofObject achievement +
  the verifier-internal Failure gap
* docs/atp/wolfram_assoc_wl_hang.md -- WL TFindProof's
  TimeConstraint enforcement gap on WolframAxioms/AndAssoc
* docs/atp/proof_parity_inverseofinverse.md -- WaldmeisterProcess
  vs internal preset structural comparison

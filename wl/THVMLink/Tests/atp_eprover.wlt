(* atp_eprover.wlt -- TEproverProof + TEproverProofObject wrapper
   tests.  Skips gracefully when the eprover binary isn't on PATH
   so this doesn't break CI on a fresh machine.

   E uses TPTP/SZS output (--proof-object --tstp-format), so the
   downstream parsing path is shared with Vampire.  *)

eproverAvailable := FileExistsQ["/opt/homebrew/bin/eprover"] ||
    FileExistsQ["/usr/local/bin/eprover"];

(* === path-form === *)

VerificationTest[
    If[ ! eproverAvailable, "Proved",
        Module[{r = TEproverProof[FileNameJoin[{Directory[], "tools",
                "baselines", "vampire_tptp",
                "AbelianGroupAxioms__InverseOfInverse.p"}],
                TimeConstraint -> 8]},
            r["Status"]]],
    "Proved",
    TestID -> "Eprover/path-form/AbelianGroup_InverseOfInverse"
]

(* === two-arg form === *)

VerificationTest[
    If[ ! eproverAvailable, "Proved",
        Module[{r = TEproverProof["AbelianGroupAxioms",
                "InverseOfInverse", TimeConstraint -> 8]},
            r["Status"]]],
    "Proved",
    TestID -> "Eprover/two-arg/AbelianGroup_InverseOfInverse"
]

(* === Method -> EproverProcess dispatch === *)

VerificationTest[
    If[ ! eproverAvailable, "SZS",
        Module[{r = TFindProof["InverseOfInverse",
                "AbelianGroupAxioms",
                Method -> "EproverProcess",
                TimeConstraint -> 8]},
            r["Backend"]]],
    "SZS",
    TestID -> "Eprover/dispatch/EproverProcess"
]

(* === Lift path for E-prover === *)

VerificationTest[
    If[ ! eproverAvailable, {ProofObject, Function},
        Module[{po = TEproverProofObject["AbelianGroupAxioms",
                "InverseOfInverse",
                TimeConstraint -> 8,
                "LiftToProofObject" -> True]},
            {Head[po], Quiet @ Head @ po["ProofFunction"]}]],
    {ProofObject, Function},
    TestID -> "Eprover/lift/AbelianGroup_InverseOfInverse"
]

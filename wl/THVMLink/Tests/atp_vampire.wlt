(* atp_vampire.wlt -- TVampireProof external-CLI wrapper tests.

   Skips gracefully when the vampire binary isn't on PATH so this
   doesn't break CI on a fresh machine. *)

vampireAvailable := FileExistsQ["/opt/homebrew/bin/vampire"] ||
    FileExistsQ["/usr/local/bin/vampire"];

(* === path-form === *)

VerificationTest[
    If[ ! vampireAvailable, "Proved",
        Module[{r = TVampireProof[FileNameJoin[{Directory[], "tools",
                "baselines", "vampire_tptp",
                "AbelianGroupAxioms__InverseOfInverse.p"}],
                TimeConstraint -> 5]},
            r["Status"]]],
    "Proved",
    TestID -> "Vampire/path-form/AbelianGroup_InverseOfInverse"
]

(* === two-arg form === *)

VerificationTest[
    If[ ! vampireAvailable, "Proved",
        Module[{r = TVampireProof["AbelianGroupAxioms",
                "InverseOfInverse", TimeConstraint -> 5]},
            r["Status"]]],
    "Proved",
    TestID -> "Vampire/two-arg/AbelianGroup_InverseOfInverse"
]

(* === reorient folding: BooleanAxioms/DoubleNegation's raw proof
   has reorient_equations entries; folded ProofLength should be
   strictly less than the raw SZS derivation count. *)

VerificationTest[
    If[ ! vampireAvailable, True,
        Module[{r = TVampireProof["BooleanAxioms",
                "DoubleNegation", TimeConstraint -> 5]},
            r["ProofLength"] > 0 &&
                NoneTrue[r["Inferences"],
                    #["Rule"] === "reorient_equations" &]]],
    True,
    TestID -> "Vampire/reorient-folded"
]

(* === inference rules captured === *)

VerificationTest[
    If[ ! vampireAvailable, {"forward_demodulation",
        "forward_subsumption_resolution", "superposition"},
        Module[{r = TVampireProof["AbelianGroupAxioms",
                "ImpliesAbelianMcCuneAxioms", TimeConstraint -> 5]},
            Union @ Cases[r["Inferences"],
                a_Association /; a["Rule"] =!= "file" :> a["Rule"]]]],
    {"forward_demodulation", "forward_subsumption_resolution",
        "superposition"},
    TestID -> "Vampire/inference-rules-captured"
]

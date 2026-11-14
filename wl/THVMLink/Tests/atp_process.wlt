(* atp_process.wlt -- the problem files the process methods (VampireProcess, EproverProcess,
   TweeProcess, WaldmeisterProcess) write for a named theorem.  They come from the theory
   itself, so an installed paclet needs no problem files and the working directory does not
   matter.  No prover binary is needed. *)

VerificationTest[
    WolframInstitute`THVMLink`ATP`Private`tptpGenerateProblemFor["AbelianGroupAxioms", "InverseOfInverse"],
    "% TPTP problem: AbelianGroupAxioms::InverseOfInverse\n% axioms = 4, conjuncts total = 1\ncnf(ax1, axiom, and(X1,and(X2,X3)) = and(and(X1,X2),X3)).\ncnf(ax2, axiom, and(X1,X2) = and(X2,X1)).\ncnf(ax3, axiom, and(X1,op_overtilde(k1)) = X1).\ncnf(ax4, axiom, and(X1,not(X1)) = op_overtilde(k1)).\ncnf(goal, negated_conjecture, not(not(sk_c1)) != sk_c1).\n",
    TestID -> "ATP/process/tptp-problem-of-named-theorem"
]

(* A theorem of several conjuncts is one conjecture, their conjunction, so the prover proves
   them all in one run. *)
VerificationTest[
    With[{text = WolframInstitute`THVMLink`ATP`Private`tptpGenerateProblemFor["BooleanAxioms", "DeMorgan"]},
        {StringContainsQ[text, "conjuncts total = 2"],
         StringContainsQ[text, "fof(goal, conjecture, (not(or(sk_c1,sk_c2)) = and(not(sk_c1),not(sk_c2))) & (not(and(sk_c1,sk_c2)) = or(not(sk_c1),not(sk_c2))))."]}],
    {True, True},
    TestID -> "ATP/process/tptp-conjunction-of-conjuncts"
]

VerificationTest[
    Replace[WolframInstitute`THVMLink`ATP`Private`tptpGenerateProblemFor["AbelianGroupAxioms", "NoSuchTheorem"], f_Failure :> {f[[1]], f[[2, "Tag"]]}],
    {"TFindProof", "nothm"},
    {TFindProof::nothm},
    TestID -> "ATP/process/unknown-theorem-fails"
]

(* The file exists while the prover runs, from any working directory, and is removed after. *)
VerificationTest[
    Block[{seen},
        WithCleanup[
            SetDirectory[$TemporaryDirectory],
            {WolframInstitute`THVMLink`ATP`Private`atpWithTheoremFile["AbelianGroupAxioms", "InverseOfInverse", "p",
                (seen = #; StringStartsQ[Import[#, "Text"], "% TPTP problem"]) &],
             FileExistsQ[seen]},
            ResetDirectory[]]],
    {True, False},
    TestID -> "ATP/process/tptp-file-written-and-removed"
]

VerificationTest[
    Block[{seen},
        {WolframInstitute`THVMLink`ATP`Private`atpWithTheoremFile["AbelianGroupAxioms", "InverseOfInverse", "pr",
            (seen = #; StringStartsQ[Import[#, "Text"], "NAME"]) &],
         FileExistsQ[seen]}],
    {True, False},
    TestID -> "ATP/process/pr-file-written-and-removed"
]

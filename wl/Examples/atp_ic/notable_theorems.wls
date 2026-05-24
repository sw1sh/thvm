#!/usr/bin/env wolframscript
(* notable_theorems.wls -- prove a set of AxiomaticTheory
   NotableTheorems with thvm's C ATP completion engine, each with an
   explicit Method.  Every case here verifies: the returned
   ProofObject(s) pass WL's own ProofObject["ProofFunction"] check.

   The Method column shows the saturator config used.  Automatic is
   the proven default (GT critical-pair weight, KBO, identity
   precedence).  MaxWallSeconds bounds the completion run so a case
   that would otherwise saturate forever fails gracefully instead of
   wedging the kernel.

   Run from the repo root:
       wolframscript -f wl/Examples/atp_ic/notable_theorems.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"];

(* {theory, theorem, Method, MaxWallSeconds}

   Automatic is a strategy schedule (Waldmeister-style portfolio): it
   tries Mix2 weight, then LPO + structure-driven AutoPrecedence, then
   GT weight, then the MNF goal-directed front search, returning the
   first that proves+verifies.  So the combinator case BCKWToSKI --
   which needs LPO (KBO cannot orient the variable-duplicating S/W
   rules) -- proves under plain Automatic; the divergent Y axiom
   (Y x == x (Y x), absent from the goal) is auto-dropped by the
   relevance filter so completion terminates; and a symmetric goal
   that no completion config can close falls through to GoalDirected. *)
$cases = {
    {"BooleanAxioms",    "DeMorgan",                        Automatic, 30},
    {"MeredithAxioms",   "ImpliesWolframAxioms",            Automatic, 30},
    {"MeredithAxioms",   "ImpliesWolframAlternateAxioms",   Automatic, 30},
    {"MeredithAxioms",   "ImpliesWolframCommutativeAxioms", Automatic, 30},
    {"MeredithAxioms",   "AndAssociativity",                Automatic, 30},
    {"MeredithAxioms",   "OrAssociativity",                 Automatic, 30},
    {"MeredithAxioms",   "ImpliesHillmanAxioms",            Automatic, 30},
    {"CombinatorAxioms", "BCKWToSKI",                       Automatic, 30},
    (* Symmetric goals: the two sides never share a single normal
       form, so completion alone cannot close them.  "GoalDirected"
       runs the MNF bidirectional front search, whose collision chain
       now resolves into a verifying critical-pair-lemma proof.  These
       also prove under plain Automatic (the schedule falls through to
       GoalDirected after the completion configs), just slower. *)
    {"BooleanAxioms",    "Noncontradiction",                "GoalDirected", 30},
    {"BooleanAxioms",    "ExcludedMiddle",                  "GoalDirected", 30},
    {"BooleanAxioms",    "DoubleNegation",                  "GoalDirected", 30},
    {"ShefferAxioms",    "Commutativity",                   "GoalDirected", 30},
    {"CombinatorAxioms", "SKIToBCKW",                       "GoalDirected", 30}
};

(* run one case; return {head, verifyHeads, seconds}. *)
runCase[theory_, thm_, method_, wall_] := Module[{t, r, verify},
    {t, r} = AbsoluteTiming @ TimeConstrained[
        TFindProof[thm, theory,
            Method -> method, MaxWallSeconds -> N[wall]],
        wall + 30, $TimedOut];
    verify = Which[
        Head[r] === ProofObject,
            {Head @ Quiet @ Check[r["ProofFunction"][r["Theorems"]], $Failed]},
        ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
            Quiet @ Check[
                Map[Head[#["ProofFunction"][#["Theorems"]]] &, r], {$Failed}],
        True, {}];
    {Head[r], verify, Round[t, 0.01]}
];

print["theory/theorem", "\t", "method", "\t", "head", "\t",
    "verify", "\t", "t(s)"];
Scan[
    Function[case,
        Module[{res = runCase @@ case},
            print[case[[1]], "/", case[[2]],
                "\t", ToString[case[[3]]],
                "\t", ToString[res[[1]]],
                "\t", ToString[res[[2]]],
                "\t", ToString[res[[3]]]]
        ]
    ],
    $cases
];

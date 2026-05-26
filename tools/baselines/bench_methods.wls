#!/usr/bin/env wolframscript
(* tools/baselines/bench_methods.wls -- compare TFindProof Method
   configs on a list of named theorems and surface the per-config
   wall-time + outcome.

   Uses the iter-30-32 return specs (AppliedMethod / WallTime /
   PortfolioTrace) so a portfolio's per-slice cost is visible.

   Usage:
     wolframscript -f tools/baselines/bench_methods.wls

   Edit $theorems / $methods below for a different sweep.
*)

PacletDirectoryLoad["wl/THVMLink"];
<< THVMLink`ATP`;
print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"];

$theorems = {
    {"AbelianGroupAxioms", "InverseOfInverse"},
    {"BooleanAxioms", "AndCommutativity"}
};

(* Keep the method set small + simple-config-first; large portfolio
   sweeps over many theorems have hit kernel SIGSEGV in this paclet
   when configs that fail-fast cycle through several portfolio slices
   in sequence.  Add more once that bug is investigated. *)
$methods = {
    Automatic,
    {"Completion", "CriticalPairWeight" -> "Twee"},
    {"Completion", "CriticalPairWeight" -> "Staggered",
        "FifoTiebreak" -> True}
};

$budget = 5;

print["theorem", "\t", "method", "\t", "outcome", "\t", "wall_s",
    "\t", "applied"];

Do[
    Module[{benchThm, benchTheory, proof, headStr, walls, applied},
        benchThm = benchCase[[2]];
        benchTheory = benchCase[[1]];
        proof = Quiet @ TFindProof[benchThm, benchTheory,
            Method -> benchM, TimeConstraint -> $budget];
        headStr = If[Head[proof] === ProofObject, "ProofObject", "Failed"];
        walls = Quiet @ TFindProof[benchThm, benchTheory, "WallTime",
            Method -> benchM, TimeConstraint -> 1];
        walls = If[NumericQ[walls], Round[walls, 0.01], -1];
        applied = Quiet @ TFindProof[benchThm, benchTheory, "AppliedMethod",
            Method -> benchM, TimeConstraint -> 1];
        applied = If[applied === Automatic || (! ListQ[applied] && ! StringQ[applied]),
            "n/a", ToString[InputForm @ applied]];
        print[benchTheory, "/", benchThm, "\t",
            ToString[InputForm @ benchM], "\t",
            headStr, "\t", walls, "\t", applied]
    ],
    {benchCase, $theorems}, {benchM, $methods}
];

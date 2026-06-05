#!/usr/bin/env wolframscript
(* tools/baselines/bench_methods.wls -- compare TFindProof Method
   configs on a list of named theorems and surface the per-config
   wall-time + outcome.

   Uses the iter-30-32 return specs (AppliedMethod / WallTime /
   PortfolioTrace) so a portfolio's per-slice cost is visible.
   Iter 45-52 added the named presets (Twee / EProver / AllPresets /
   VampirePortfolioCompact); the default method set below now exercises
   each preset alongside the bare Automatic dispatch.

   Usage:
     wolframscript -f tools/baselines/bench_methods.wls

   Edit $theorems / $methods below for a different sweep.
*)

PacletDirectoryLoad["wl/THVMLink"];
<< THVMLink`ATP`;
$theorems = {
    {"AbelianGroupAxioms", "InverseOfInverse"},
    {"BooleanAxioms", "AndCommutativity"},
    {"GroupAxioms", "LeftIdentity"}
};

(* Named-preset surface (post-iter-52).  Each preset bundles a specific
   strategy; "VampirePortfolioCompact" + "AllPresets" expand into
   schedules.  The iter-40 SIGSEGV fix means cycling fail-fast slices
   is safe again -- a portfolio sweep no longer crashes the kernel.  *)
$methods = {
    Automatic,
    "Waldmeister",
    "VampireUEQ",
    "Twee",
    "EProver",
    "VampirePortfolioCompact",
    "AllPresets"
};

$budget = 5;

Print["theorem", "\t", "method", "\t", "outcome", "\t", "wall_s",
    "\t", "applied"];

Do[
    Module[{benchThm, benchTheory, proof, headStr, walls, applied},
        benchThm = benchCase[[2]];
        benchTheory = benchCase[[1]];
        proof = Quiet @ TFindProof[benchThm, benchTheory,
            Method -> benchM, TimeConstraint -> $budget];
        headStr = If[Head[proof] === ProofObject, "ProofObject", "Failed"];
        walls = Quiet @ TFindProof[benchThm, benchTheory, "WallTime",
            Method -> benchM, TimeConstraint -> $budget];
        walls = If[NumericQ[walls], Round[walls, 0.01], -1];
        applied = Quiet @ TFindProof[benchThm, benchTheory, "AppliedMethod",
            Method -> benchM, TimeConstraint -> $budget];
        applied = If[
            applied === Automatic || (! ListQ[applied] && ! StringQ[applied]),
            "n/a", ToString[InputForm @ applied]];
        Print[benchTheory, "/", benchThm, "\t",
            ToString[InputForm @ benchM], "\t",
            headStr, "\t", walls, "\t", applied]
    ],
    {benchCase, $theorems}, {benchM, $methods}
];

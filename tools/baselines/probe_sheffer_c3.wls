#!/usr/bin/env wolframscript
(* Focused probe: every multi-conjunct *ImpliesShefferAxioms residual
   has the same 3rd conjunct (the long Sheffer 3rd axiom).  Vampire
   cracks several of these via `tgt=ground` (goal-grounding) + LPO
   + sp=reverse_frequency + bd=preordered.  thvm's analogue: LPO +
   AutoPrecedence ReverseFrequency + SkolemHighest + UnfailingCP
   + BackwardDemod + RHSInterreduce + GroundJoin + AutoMaxWeight 30.

   Run each c3 variant at TC=120s. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TC = 120;
config = {"Completion", "Ordering" -> "LPO",
    "AutoPrecedence" -> "ReverseFrequency",
    "SkolemHighest" -> True,
    "UnfailingCP" -> True, "BackwardDemod" -> True,
    "RHSInterreduce" -> True, "GroundJoin" -> True,
    "AutoMaxWeight" -> 30};

cases = {
    {"HillmanAxioms", "ImpliesShefferAxioms"},
    {"MeredithAxioms", "ImpliesShefferAxioms"},
    {"WolframAlternateAxioms", "ImpliesShefferAxioms"},
    {"WolframAxioms", "ImpliesShefferAxioms"},
    {"WolframCommutativeAxioms", "ImpliesShefferAxioms"}
};

Scan[Function[case,
    Module[{theory, thm, cj, target, t, r, status, proofLen},
        {theory, thm} = case;
        cj = AxiomaticTheory[theory, "NotableTheorems"][thm];
        target = If[ListQ[cj], cj[[3]], cj];
        {t, r} = AbsoluteTiming @ TimeConstrained[
            TFindProof[target, theory, Method -> config,
                TimeConstraint -> N[TC]],
            TC + 15, $TimedOut];
        status = Which[
            Head[r] === ProofObject, "PROVED",
            r === $TimedOut, "TimedOut",
            True, "Failed"];
        proofLen = Which[
            Head[r] === ProofObject,
                ToString @ Quiet @ Check[r["ProofLength"], "-"],
            True, "-"];
        Print[StringPadRight[theory, 28], " c3   ",
            StringPadRight[status, 10], Round[t, 0.1], "s  pl=", proofLen]
    ]], cases];

#!/usr/bin/env wolframscript
(* Probe: does "AutoPrecedence" -> "ReverseFrequency" combined with
   "KboWeightScheme" -> "InvPrecedence" crack any of the 6 stuck
   NotableTheorems that solo reverse_frequency missed?  Mirrors the
   Vampire winning shape `nwc=N:kws=inv_precedence:to=kbo:
   sp=reverse_frequency` minus the FOL-clause-only nwc flag (UEQ has
   no non-unit clauses).  TC=60s. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TC = 60;
config = {"Completion", "Ordering" -> "KBO",
    "AutoPrecedence" -> "ReverseFrequency",
    "KboWeightScheme" -> "InvPrecedence",
    "GroundJoin" -> True, "BackwardDemod" -> True,
    "RHSInterreduce" -> True, "UnfailingCP" -> True};

candidates = {
    {"ShefferAxioms", "AndAssociativity"},
    {"ShefferAxioms", "OrAssociativity"},
    {"WolframAxioms", "ImpliesMeredithAxioms"},
    {"WolframCommutativeAxioms", "ImpliesMeredithAxioms"},
    {"HillmanAxioms", "ImpliesShefferAxioms"},
    {"MeredithAxioms", "ImpliesShefferAxioms"}
};

Scan[Function[case,
    Module[{theory, thm, t, r, status, proofLen},
        {theory, thm} = case;
        {t, r} = AbsoluteTiming @ TimeConstrained[
            TFindProof[thm, theory, Method -> config, TimeConstraint -> N[TC]],
            TC + 5, $TimedOut];
        status = Which[
            Head[r] === ProofObject, "PROVED",
            ListQ[r] && AllTrue[r, Head[#] === ProofObject &], "PROVED",
            r === $TimedOut, "TimedOut",
            True, "Failed"];
        proofLen = Which[
            Head[r] === ProofObject,
                ToString @ Quiet @ Check[r["ProofLength"], "-"],
            ListQ[r] && AllTrue[r, Head[#] === ProofObject &],
                ToString[Total[Quiet @ Check[#["ProofLength"], 0] & /@ r]],
            True, "-"];
        Print[StringPadRight[theory, 28], StringPadRight[thm, 36],
            StringPadRight[status, 9], Round[t, 0.01], "s  pl=", proofLen]
    ]], candidates];

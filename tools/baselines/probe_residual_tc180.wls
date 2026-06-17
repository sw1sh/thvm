#!/usr/bin/env wolframscript
(* Budget-bound vs fundamentally-stuck probe.  Same combo config as
   probe_kws_inv_precedence.wls, but TC=180s instead of 60s.  If any
   case cracks, the residuals are saturation-rate-bound (engine perf
   matters).  If 0/6 still, the search is saturating to a fixed point
   that doesn't include the goal -- needs heuristic/strategy work,
   not more wall. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TC = 180;
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
            TC + 10, $TimedOut];
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

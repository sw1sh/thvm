#!/usr/bin/env wolframscript
(* Probe: does the new "AutoPrecedence" -> "ReverseFrequency" port
   crack any of the 22 still-missing NotableTheorems?  TC=60s each
   to match the previous sweep budget.  Output one TSV line per
   theorem in run_thvm_one.wls's format.  Use single-kernel (this
   script is short -- if a heavy case OOMs, only the probe dies). *)
PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

TC = 60;
config = {"Completion", "Ordering" -> "LPO",
    "AutoPrecedence" -> "ReverseFrequency",
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

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"];

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
        print[StringPadRight[theory, 28], StringPadRight[thm, 36],
            StringPadRight[status, 9], Round[t, 0.01], "s  pl=", proofLen]
    ]], candidates];

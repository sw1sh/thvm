#!/usr/bin/env wolframscript
(* Diverging-queue lever probe.  ShefferAxioms/AndAssociativity
   diverges at LPO+AutoPrec (Rules=119, QueueSize=58k at TC=60s).
   Try aggressive CP-cap + ordering tweaks that constrain breadth
   instead of search-deeper-with-bigger-budget. *)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];

TC = 60;
configs = {
    {"LPO+autoprec+MaxW=15",
        {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
         "MaxWeight" -> 15, "UnfailingCP" -> True,
         "BackwardDemod" -> True, "RHSInterreduce" -> True}},
    {"LPO+autoprec+AutoMaxW=20",
        {"Completion", "Ordering" -> "LPO", "AutoPrecedence" -> True,
         "AutoMaxWeight" -> 20, "UnfailingCP" -> True,
         "BackwardDemod" -> True, "RHSInterreduce" -> True}},
    {"KBO+autoprec+GJ+CPSI",
        {"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True,
         "GroundJoin" -> True, "Connectedness" -> True,
         "CPSetInterreduce" -> True, "UnfailingCP" -> True,
         "BackwardDemod" -> True}},
    {"KBO+autoprec+SR=51+CPSI (Waldmeister-shape)",
        {"Completion", "Ordering" -> "KBO", "AutoPrecedence" -> True,
         "CriticalPairWeight" -> "Mix", "SelectionRatio" -> 51,
         "RHSInterreduce" -> True, "UnfailingCP" -> True,
         "CPSetInterreduce" -> True}},
    {"LPO+nand-lowest (reverse WM precedence)",
        {"Completion", "Ordering" -> "LPO",
         "Precedence" -> {And, Or, Nand}, "UnfailingCP" -> True,
         "BackwardDemod" -> True, "RHSInterreduce" -> True}},
    {"Goal+LPO+autoprec",
        {"Completion", "CriticalPairWeight" -> "Goal",
         "Ordering" -> "LPO", "AutoPrecedence" -> True,
         "UnfailingCP" -> True, "BackwardDemod" -> True}},
    {"Twee preset",
        "Twee"},
    {"VampireUEQ preset",
        "VampireUEQ"}
};

Scan[Function[entry,
    Module[{label, cfg, t, r, status, proofLen, stats},
        {label, cfg} = entry;
        {t, r} = AbsoluteTiming @ TimeConstrained[
            TFindProof["AndAssociativity", "ShefferAxioms",
                Method -> cfg, TimeConstraint -> N[TC]],
            TC + 15, $TimedOut];
        status = Which[
            Head[r] === ProofObject, "PROVED",
            ListQ[r] && AllTrue[r, Head[#] === ProofObject &], "PROVED",
            r === $TimedOut, "TimedOut",
            True, "Failed"];
        proofLen = Which[
            Head[r] === ProofObject,
                ToString @ Quiet @ Check[r["ProofLength"], "-"],
            True, "-"];
        Print[StringPadRight[label, 50],
            StringPadRight[status, 10], Round[t, 0.1], "s  pl=", proofLen]
    ]], configs];

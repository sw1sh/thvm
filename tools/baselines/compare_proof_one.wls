#!/usr/bin/env wolframscript
(* Side-by-side proof-level comparison of a thvm preset vs the
   Vampire CLI on a single (theory, thm).  Reports for each side:
     - Status (Proved / TimedOut / Failed)
     - Wall seconds
     - Proof length
     - For Vampire: rule histogram (#superp, #demod, #FSR)
     - For thvm: rule histogram (#CPL, #SubstitutionLemma, #Conclusion)
     - DELTA: rule-count diffs that surface the per-step gap.

   Output: one TSV line (compact, machine-readable) + a verbose
   markdown block to stderr.  Usage:
       compare_proof_one.wls <theory> <thm> <preset> <TC_seconds>

   Example:
       compare_proof_one.wls AbelianGroupAxioms \\
           ImpliesAbelianMcCuneAxioms VampireUEQ 10
*)

SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];
PacletDirectoryLoad["/Users/swish/src/wolfram/AISkills/examples/WolframParser"];
Get["THVMLink`"];
Needs["Wolfram`Parser`"];
$RecursionLimit = 16384;

args = $ScriptCommandLine;
theory = args[[2]];
thm    = args[[3]];
preset = args[[4]];
TC     = ToExpression[args[[5]]];

emit[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"];

(* ===================== thvm side ===================== *)
(* WARMUP: TFindProof's first call in a fresh kernel pays a ~1.6s
   LibraryFunctionLoad / discrim-tree init that subsequent calls
   amortise (127x cold/warm).  Vampire CLI is a separate process per
   call, so its 50ms IS its cold-start.  Charge an apples-to-apples
   wall by warming the bench harness first. *)
Print["[thvm:warmup] priming LibraryFunctionLoad + caches..."];
Quiet @ TimeConstrained[
    TFindProof["InverseOfInverse", "GroupAxioms",
        Method -> "Completion", TimeConstraint -> 2.0],
    3, $TimedOut];

Print["[thvm:", preset, "] running..."];
{thvmT, thvmR} = AbsoluteTiming @ TimeConstrained[
    TFindProof[thm, theory, Method -> preset, TimeConstraint -> N[TC]],
    TC + 10, $TimedOut];
thvmStatus = Which[
    Head[thvmR] === ProofObject, "Proved",
    ListQ[thvmR] && AllTrue[thvmR, Head[#] === ProofObject &], "Proved",
    thvmR === $TimedOut, "TimedOut",
    True, "Failed"];
thvmPL = Which[
    Head[thvmR] === ProofObject,
        Quiet @ Check[thvmR["ProofLength"], 0],
    True, 0];
(* Walk the thvm proof DAG and count rule types. *)
thvmRuleCounts = If[ Head[thvmR] === ProofObject,
    Module[{ds = Quiet @ Check[
            thvmR["ProofDataset"]["Properties"], <||>], allKeys, types},
        allKeys = Cases[Keys[Quiet @ Check[
            thvmR["ProofObject"][[-1, "Proof"]],
            thvmR[[-1, "Proof"]]]],
            {t_, _Integer} :> t];
        Counts[allKeys]],
    <||>];

(* ===================== vampire side ===================== *)
Print["[vampire-cli] running..."];
vampR = TVampireProof[theory, thm, TimeConstraint -> TC];
vampStatus = vampR["Status"];
vampPL     = vampR["ProofLength"];
vampStrat  = vampR["Strategy"];
vampSecs   = vampR["Seconds"];
vampRuleCounts = If[ vampStatus === "Proved",
    KeySort @ Counts[#["Rule"] & /@ vampR["Inferences"]],
    <||>];

(* ===================== report ===================== *)
Print["============================================================"];
Print["theory/thm:    ", theory, "/", thm];
Print["preset:        ", preset];
Print["TC:            ", TC, "s"];
Print[];
Print["               thvm                 vampire-cli"];
Print["status         ", StringPadRight[ToString[thvmStatus], 22],
    StringPadRight[ToString[vampStatus], 20]];
Print["seconds        ", StringPadRight[ToString[Round[thvmT, 0.01]], 22],
    StringPadRight[ToString[vampSecs], 20]];
Print["proofLen       ", StringPadRight[ToString[thvmPL], 22],
    StringPadRight[ToString[vampPL], 20]];
Print["strategy       ", StringPadRight[preset, 22],
    Short[ToString[vampStrat], 40]];
Print[];
Print["thvm rule histogram:"];
Scan[(Print["  ", StringPadRight[ToString[#[[1]]], 25], #[[2]]]) &,
    Normal @ KeySort @ thvmRuleCounts];
Print["vampire rule histogram:"];
Scan[(Print["  ", StringPadRight[ToString[#[[1]]], 25], #[[2]]]) &,
    Normal @ vampRuleCounts];
Print[];
(* Speed ratio *)
If[ thvmStatus === "Proved" && vampStatus === "Proved" && vampSecs > 0,
    Print["thvm/vampire wall ratio:    ", Round[thvmT / vampSecs, 0.1], "x"];
    Print["thvm/vampire proofLen ratio: ", Round[thvmPL / vampPL, 0.01], "x"]];
Print["============================================================"];

(* TSV line (one row per call): theory thm preset thvmStatus thvmSecs thvmPL
                                vampStatus vampSecs vampPL ratio *)
emit[theory, "\t", thm, "\t", preset, "\t",
    thvmStatus, "\t", Round[thvmT, 0.01], "\t", thvmPL, "\t",
    vampStatus, "\t", vampSecs, "\t", vampPL, "\t",
    If[ thvmStatus === "Proved" && vampStatus === "Proved" && vampSecs > 0,
        ToString @ N[Round[thvmT / vampSecs, 0.1]],
        "-"]];

#!/usr/bin/env wolframscript
(* WL gate for the wmo self-root face-flip fix (Boolean Absorption 107
   leaf-list partner ordering).  Proves the must-hold theorems on
   Method->"Waldmeister", reports row counts (CriticalPairLemma /
   SubstitutionLemma) and verify status.  Expected (unchanged baselines):
     McCune-II (McCuneAxioms/InverseOfInverse) 43 rows (32 CPL + 8 SL) Success
     Group IOC 20 (9 CPL + 6 SL), ImpliesMcCune 28 (11 + 12) -- exact
     plain-WM ExcludedMiddle 14, Noncontradiction 14, EqualityOfInverses 45 Success *)
SetDirectory["/Users/swish/src/thvm"];
PacletDirectoryLoad["wl/THVMLink"];      (* repo paclet FIRST *)
Get["WolframInstitute`THVMLink`"];
$RecursionLimit = 16384;

rowCount[po_, key_] := Block[{ds = po["ProofDataset"]},
    ds = If[AssociationQ[ds], ds, Association @ Normal[ds]];
    Length @ Select[Keys[ds], First[#] === key &]];

verifyOK[po_] := TrueQ @ Quiet @ Check[
    MatchQ[po["ProofFunction"][po["Theorems"]], _?(# =!= $Failed &)], False];

probe[thm_, theory_, method_] := Block[{t, r, cpl, sl, tot, vok},
    {t, r} = AbsoluteTiming @ TimeConstrained[
        TFindProof[thm, theory, Method -> method, TimeConstraint -> 30.0],
        40, $TimedOut];
    If[ MatchQ[r, _ProofObject],
        cpl = rowCount[r, "CriticalPairLemma"];
        sl  = rowCount[r, "SubstitutionLemma"];
        tot = Length @ Block[{ds = r["ProofDataset"]},
            Keys @ If[AssociationQ[ds], ds, Association @ Normal[ds]]];
        vok = verifyOK[r];
        WriteString["stdout", theory, "/", thm, " [", ToString[method],
            "] => ", ToString[tot], " rows (", ToString[cpl], " CPL + ",
            ToString[sl], " SL), verify=", If[vok, "Success", "FAIL"],
            ", wall=", ToString[Round[t, 0.01]], "s\n"];
        vok,
        WriteString["stdout", theory, "/", thm, " [", ToString[method],
            "] => ", ToString[Head[r]], " (NOT ProofObject)\n"];
        False]];

(* warm the kernel / loader *)
Quiet @ TimeConstrained[
    TFindProof["InverseOfInverse", "GroupAxioms", Method -> "Waldmeister",
        TimeConstraint -> 2.0], 5, $TimedOut];

WriteString["stdout", "=== WL gate (Waldmeister preset) ===\n"];
probe["InverseOfInverse", "McCuneAxioms", "Waldmeister"];
probe["InverseOfComposite", "GroupAxioms", "Waldmeister"];
probe["ImpliesMcCuneAxioms", "GroupAxioms", "Waldmeister"];
probe["ExcludedMiddle", "BooleanAxioms", "Waldmeister"];
probe["Noncontradiction", "BooleanAxioms", "Waldmeister"];
probe["EqualityOfInverses", "McCuneAxioms", "Waldmeister"];
WriteString["stdout", "=== gate done ===\n"];

#!/usr/bin/env wolframscript
(* atp_wolfram_to_sheffer.wls -- attempt to prove each of Sheffer's
   three axioms from Wolfram's 2000 single-equation axiomatisation
   of Boolean algebra, using TATP[].

   AxiomaticTheory["WolframAxioms"]["Axioms"] is the single Wolfram
   equation; AxiomaticTheory["ShefferAxioms"]["Axioms"] is the three
   Sheffer-stroke axioms.  This script strips ForAll, converts formal
   vars (\[FormalA], \[FormalB], \[FormalC]) to Pattern form for the
   axiom and to skolem constants (aSk, bSk, cSk) for each goal, then
   runs TATP[] in expression form (C-direct cp-gen + rewrite) on
   each Sheffer axiom against the Wolfram axiom.

   Run from the repo root:
       wolframscript -f wl/Examples/atp_wolfram_to_sheffer.wls

   Honest expectation: under our 32-step bench budget every Sheffer
   goal TIMEs OUT; lemma discovery from the depth-4 Wolfram axiom
   exceeds the budget.  See docs/plans/wolfram_axiom_design.md and
   docs/plans/corpus_expansion_findings.md (stage 10c update) for
   the cell-budget analysis.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout",
  StringJoin @@ Map[ToString, {args}], "\n"
];

(* --- Retrieve and reshape the axioms ------------------------------ *)
(* AxiomaticTheory[name] returns the axioms List directly -- no
   ["Axioms"] indexing.  WolframAxioms is a 1-element list (one
   ForAll); ShefferAxioms is a 3-element list. *)

$wolframRaw = AxiomaticTheory["WolframAxioms"];
$shefferRaw = AxiomaticTheory["ShefferAxioms"];

(* Strip ForAll[...]. *)
stripForAll[expr_] := expr /. ForAll[_, eq_] :> eq;

(* Substitution rules.  The Wolfram axiom keeps its variables as
   Pattern[name, Blank[]] so TATP's encoder treats them as FVRs.
   The Sheffer goals use concrete skolem symbols for ground proof. *)
formalToPattern = {
  \[FormalA] -> Pattern[a, Blank[]],
  \[FormalB] -> Pattern[b, Blank[]],
  \[FormalC] -> Pattern[c, Blank[]]
};

formalToSkolem = {
  \[FormalA] -> aSk,
  \[FormalB] -> bSk,
  \[FormalC] -> cSk
};

$wolframAxiomEq = stripForAll[$wolframRaw[[1]]] /. formalToPattern;
$shefferGoalEqs = (stripForAll /@ $shefferRaw) /. formalToSkolem;

debugPrint["Wolfram axiom (with x_, y_, z_ pattern vars):"];
debugPrint["  ", $wolframAxiomEq];
debugPrint[""];
debugPrint["Sheffer goals (skolemized constants aSk, bSk, cSk):"];
Do[
  debugPrint["  S", i, ":  ", $shefferGoalEqs[[i]]],
  {i, Length[$shefferGoalEqs]}
];
debugPrint[""];

(* --- Attempt each goal -------------------------------------------- *)
(* TATP has HoldAll, so we use With[] to inject the literal axiom +
   goal expressions into the TATP call.  MaxSteps=64 doubles the
   bench budget but stays well under HEAP_CAP for the C-direct
   path. *)

debugPrint["Running TATP[Wolfram |- S_i] for i = 1..", Length[$shefferGoalEqs], " ..."];
debugPrint[""];

results = Table[
  With[{ax = $wolframAxiomEq, gl = $shefferGoalEqs[[i]]},
    TATP[{ax}, gl, MaxSteps -> 64]
  ],
  {i, Length[$shefferGoalEqs]}
];

debugPrint["Results:"];
Do[
  Module[{r = results[[i]]},
    debugPrint["  S", i, ": Status=", r["Status"],
              "  Steps=", r["Steps"],
              "  Rules=", r["Rules"],
              "  QueueSize=", r["QueueSize"]]
  ],
  {i, Length[results]}
];
debugPrint[""];

(* --- Score --------------------------------------------------------- *)
provedCount = Count[results, _?(#["Status"] === "PROVED" &)];
debugPrint["Score: ", provedCount, " / ", Length[results],
          " Sheffer axioms PROVED from the Wolfram axiom under",
          " MaxSteps=64."];

debugPrint[""];
debugPrint["Note: Twee at --max-cps 256 proves all three; our 32-64"];
debugPrint["step budget exceeds lemma-discovery cost.  See"];
debugPrint["docs/plans/corpus_expansion_findings.md."];

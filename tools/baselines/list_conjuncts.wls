#!/usr/bin/env wolframscript
(* For each (theory, thm) on stdin (tab-separated), look up the
   conjecture under AxiomaticTheory[theory, "NotableTheorems"][thm]
   and emit its conjunct count.  Output: theory \t thm \t n_conjuncts.

   Single wolframscript call so the bench harness's per-conjunct
   loop doesn't pay 3-5s wolframscript startup per theorem just to
   query the count. *)
print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"];

input = $ScriptCommandLine[[2]];
lines = Import[input, "Lines"];
rows = StringSplit[#, "\t"] & /@ lines;

Scan[Function[r,
    Module[{theory, thm, cj, n},
        {theory, thm} = r;
        cj = AxiomaticTheory[theory, "NotableTheorems"][thm];
        n = If[ ListQ[cj], Length[cj], 1];
        print[theory, "\t", thm, "\t", n]]],
    rows];

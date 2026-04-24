(* Setup for DUP-SUP same-label annihilation.  Explicit label `7` so
   the dup and sup match.  Returns both projections so the graph
   shows DUP, SUP, and the two SUP branches. *)
TDup[7, TSup[7, TEra[], TLam[var |-> var]],
    {dp0, dp1} |-> {dp0, dp1}]

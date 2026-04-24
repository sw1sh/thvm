(* Setup for DUP-SUP same-label annihilation; explicit label `7` so
   the dup and sup match.  Returns DP0 so that reducing this term
   fires DUP-SUP and produces the left branch (ERA). *)
TDup[7, TSup[7, TEra[], TLam[var |-> var]],
    {dp0, dp1} |-> dp0]

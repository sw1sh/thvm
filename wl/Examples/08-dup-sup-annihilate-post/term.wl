(* Same setup as 07, then reduce DP0 (firing DUP-SUP).  Returns both
   projections so the graph shows the consumed DUP cell (now dashed)
   and the surviving LAM that DP1 will pick up. *)
TDup[7, TSup[7, TEra[], TLam[var |-> var]],
    {dp0, dp1} |-> (TWnf[dp0]; {dp0, dp1})]

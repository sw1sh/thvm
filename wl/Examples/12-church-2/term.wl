(* Church 2 = lam s. lam z. s (s z)
   Uses s twice, so we explicitly TDup it: !{s0, s1} = s. *)
TLam[s |->
    TDup[s, {s0, s1} |->
        TLam[z |-> TApp[s0, TApp[s1, z]]]]]

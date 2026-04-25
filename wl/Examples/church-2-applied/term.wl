(* Church 2 applied to identity and ERA: church2 f x  =  f (f x).
   The redex; reducing it fires APP-LAM and DUP-LAM. *)
With[
    {
        church2 = TLam[s,
            TDup[s, {s0, s1} |->
                TLam[z, s0[s1[z]]]]],
        f       = TLam[var, var]
    },
    church2[f][TEra[]]
]

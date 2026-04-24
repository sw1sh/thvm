(* Church 2 applied to a placeholder function and zero (here both as
   the identity lambda and ERA, just to give the reducer something
   concrete to work on).  Returns the reduction result so the graph
   shows the post-firing heap state.

       church2 f x  =  f (f x)
*)
With[{
    church2 = TLam[s |->
        TDup[s, {s0, s1} |->
            TLam[z |-> TApp[s0, TApp[s1, z]]]]],
    f = TLam[var |-> var],
    x = TEra[]
},
    TWnf[TApp[TApp[church2, f], x]]
]

(* (ERA lam) fires APP-ERA.  Return the original APP to show the
   orphan APP and the now-dangling LAM cell that was its arg. *)
With[{app = TApp[TEra[], TLam[var |-> var]]},
    TWnf[app];
    app
]

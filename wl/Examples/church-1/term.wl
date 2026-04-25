(* Church 1 = lam s. lam z. (s z)
   Single use of s, so no DUP needed. *)
TLam[s, TLam[z, TApp[s, z]]]

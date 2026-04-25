(* K = lam x. lam y. x   --   discards y, keeps x.
   y is unused so reducing (K a) b leaks the b cell, but the result
   is correctly a. *)
TLam[x, TLam[y, x]]

(* uop-mul-add: composed expression  (a * b) + c.  Two compute UOPs
   chained -- MUL feeds into ADD via the principal-cell wire. *)
With[{a = TTensor[{3}], b = TTensor[{3}], c = TTensor[{3}]},
    TUOpAdd[TUOpMul[a, b], c]
]

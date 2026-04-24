(* grad-add: gradient of (a + b) with respect to a.
   Expected after TWnf: ADD[ADD[gy, CONST(0)], MUL[a, CONST(0)]]
   which dispatches to ones_like(a). *)
With[{a = TTensor[{3}], b = TTensor[{3}]},
    TGrad[TUOpAdd[a, b], a]
]

(* grad-mul: gradient of (a * b) with respect to a -- product rule.
   Expected after TWnf: ADD[ADD[MUL[b, gy], CONST(0)], MUL[a, CONST(0)]]
   which dispatches to b. *)
With[{a = TTensor[{3}], b = TTensor[{3}]},
    TGrad[TUOpMul[a, b], a]
]

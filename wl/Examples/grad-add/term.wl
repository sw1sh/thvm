(* grad-add: gradient of (a + b) with respect to a.
   Expected after TWnf: ADD[ADD[gy, CONST(0)], MUL[a, CONST(0)]]
   which dispatches to ones_like(a). *)
With[{
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
},
    TGrad[TUOpAdd[a, b], a]
]

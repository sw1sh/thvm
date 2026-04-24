(* grad-mul: gradient of (a * b) with respect to a -- product rule.
   Expected after TWnf: ADD[ADD[MUL[b, gy], CONST(0)], MUL[a, CONST(0)]]
   which dispatches to b. *)
With[{
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"],
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"]
},
    TGrad[TUOpMul[a, b], a]
]

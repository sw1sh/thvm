(* grad-x-times-x: gradient of (a * a) -- both branches of the
   product rule hit the target.  Expected after TWnf:
     ADD[ADD[MUL[a, gy], MUL[a, gy]], MUL[a, CONST(0)]]
   which dispatches to 2a. *)
With[{
    a = TTensorCreate @ NumericArray[{2.0, 3.0, 5.0}, "Real32"]
},
    TGrad[TUOpMul[a, a], a]
]
